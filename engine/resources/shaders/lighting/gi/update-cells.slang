import modules.base;
import modules.gi;
import modules.materials;
import modules.lighting;
import modules.gbuffer;
import modules.camera;
import modules.raytracing;
import modules.rng;
import modules.compute_fullscreen;

public static const uint MaxRaysPerProbe = ScreenProbeSize*ScreenProbeSize;
public static const float ProbeCellSize = 1.0f;

public struct RayArray {
    public uint count;
    public SpawnedRay rays[];
}

struct Counts {
    uint spawnedProbes;
    uint emptyProbes;
    uint reprojectedProbes;
    uint spawnedRays;
}

struct Probes {
    RWStructuredBuffer<ScreenProbe, ScalarDataLayout> probes;
    StructuredBuffer<ScreenProbe, ScalarDataLayout> previousFrameProbes;
    RWStructuredBuffer<Counts> counts;
    RWStructuredBuffer<Atomic<uint>, ScalarDataLayout> spawnedProbes;
    RWStructuredBuffer<Atomic<uint>, ScalarDataLayout> emptyProbes;
    RWStructuredBuffer<Atomic<uint>, ScalarDataLayout> reprojectedProbes;
    RWStructuredBuffer<Atomic<uint>, ScalarDataLayout> rayDataStart; // start into rayData, per probe
    RWStructuredBuffer<RayData, ScalarDataLayout> rayData;
}

ParameterBlock<HashGrid::Block> hashGridData;
ParameterBlock<Probes> probeData;
ParameterBlock<MaterialSystem> materialSystem;
ParameterBlock<Lighting> lightData;
ParameterBlock<GBufferInputs> gBufferInputs;
ParameterBlock<Camera> cameras;
ParameterBlock<Raytracing::Data> rtData;

groupshared uint reprojectionScore;

uint packHalf(float f) {
    return asuint(half(f));
}

Optional<uint> getReprojectedProbeIndex(uint2 frameDimensions, vec2 uv, vec2 motionVector) {
    const uint probesPerWidth = (frameDimensions.x+ScreenProbeSize-1) / ScreenProbeSize;

    // reproject temporally, and check if pixel is reusable
    const vec2 reprojectedUV = uv + motionVector;
    const bool reprojectionInBounds = reprojectedUV.x >= 0.0f && reprojectedUV.x < 1.0f && reprojectedUV.y >= 0.0f && reprojectedUV.y < 1.0f;
    if(reprojectionInBounds) {
        const float2 size = float2(frameDimensions.x, frameDimensions.y);
        const int2 reprojectedPixel = int2(reprojectedUV * size);
        const uint reprojectedProbeIndex = reprojectedPixel.x / ScreenProbeSize + reprojectedPixel.y / ScreenProbeSize * probesPerWidth;
        return reprojectedProbeIndex;
    }

    return none;
}

struct DecayPushConstant {
    uint maxCellIndex;
    uint frameCount;
}

[shader("compute")]
[numthreads(256, 1, 1)]
void decayCells(uint3 coords: SV_DispatchThreadID, uniform DecayPushConstant push) {
    uint cellIndex = coords.x;

    if(cellIndex >= push.maxCellIndex) {
        return;
    }

    if(hashGridData.grids.currentFrame.isOutdated(cellIndex, push.frameCount)) {
        hashGridData.grids.currentFrame.clearCell(cellIndex);
    }
}

[shader("compute")]
[numthreads(8, 8, 1)]
void spawnScreenProbes(uint3 coords : SV_DispatchThreadID, uint localIndex : SV_GroupIndex, uint3 groupID : SV_GroupID, uniform LightingPassUniforms push) {
    // this will store the best matching pixel for this workgroup
    reprojectionScore = packHalf(65504.0f) << 16/*score*/ | 0xFFFFu /*lane*/;

    const uint probesPerWidth = (push.frameWidth + ScreenProbeSize-1) / ScreenProbeSize;
    const uint probeIndex = groupID.x + groupID.y * probesPerWidth;
    if(probeIndex >= probeData.probes.getCount()) {
        return;
    }
    const int2 pixel = coords.xy;
    const float2 size = float2(push.frameDimensions);
    const float2 uv = (pixel + float2(0.5f, 0.5f)) / size;

    const PixelInfo pixelInfo = PixelInfo(cameras.CurrentFrame(), gBufferInputs, uv);

    GroupMemoryBarrier();
    if(pixelInfo.isSky) {
        return;
    }

    // check if we can reuse a probe from the previous frame
    if(let reprojectedProbeIndex = getReprojectedProbeIndex(push.frameDimensions, uv, pixelInfo.gBuffer.motionVector.xy)) {
        const ScreenProbe reprojectedProbe = probeData.previousFrameProbes[reprojectedProbeIndex];
        const float3 dPos = reprojectedProbe.worldPos - pixelInfo.worldPosition;
        const float planeDist = abs(dot(dPos, pixelInfo.worldNormal));
        const float normalCheck = dot(pixelInfo.worldNormal, reprojectedProbe.normal);
        // TODO: is it correct to use a constant cell size here?
        if(planeDist < ProbeCellSize && normalCheck > 0.95) {
            const float dist = length(dPos);
            const uint probeScore = (packHalf(dist) << 16) | localIndex;
            InterlockedMin(reprojectionScore, probeScore);
        }
    }

    GroupMemoryBarrier();
    if(localIndex != 0) {
        return;
    }

    // at this point reprojectionScore contains the ID of the best matching pixel
    uint reusedPixel = reprojectionScore & 0xFFFFu;
    uint spawnedIndex;
    InterlockedAdd(probeData.counts[0].spawnedProbes, 1, spawnedIndex);
    probeData.spawnedProbes[spawnedIndex] = probeIndex;

    if(reusedPixel == 0xFFFFu) {
        // no reuse
        probeData.probes[probeIndex].reset(pixelInfo.worldPosition, pixelInfo.worldNormal, pixel);

        uint emptyIndex; 
        InterlockedAdd(probeData.counts[0].emptyProbes, 1, emptyIndex);
        //emptyProbes.indices[emptyIndex] = spawnedIndex;
        probeData.emptyProbes[emptyIndex] = probeIndex;
    } else {
        const int2 screenTileStart = int2(groupID.xy * uint2(ScreenProbeSize));
        const uint localPixel = reprojectionScore & 0xFFFFu;
        const int2 posInTile = int2(localPixel % ScreenProbeSize, localPixel / ScreenProbeSize);
        const int2 reprojectedPixelPos = screenTileStart + posInTile;
        const float2 reprojectedUV = (float2(reprojectedPixelPos) + float2(0.5)) / size;
        const GBuffer reprojectedGBuffer = gBufferInputs.read(reprojectedUV);

        const uint reprojectedProbeIndex = getReprojectedProbeIndex(push.frameDimensions, reprojectedUV, reprojectedGBuffer.motionVector.xy).value;
        const ScreenProbe reprojectedProbe = probeData.previousFrameProbes[reprojectedProbeIndex];

        probeData.probes[probeIndex].reprojectRadiance(reprojectedProbe);
        probeData.probes[probeIndex].worldPos = (cameras.CurrentFrame().inverseView * vec4(reprojectedGBuffer.viewPosition, 1)).xyz;

        const float3x3 inverseNormalView = transpose(inverse(float3x3(cameras.CurrentFrame().inverseView)));
        probeData.probes[probeIndex].normal = inverseNormalView * reprojectedGBuffer.viewTBN[2];
        probeData.probes[probeIndex].bestPixel = reprojectedPixelPos;

        uint reprojectedIndex;
        InterlockedAdd(probeData.counts[0].reprojectedProbes, 1, reprojectedIndex);
        probeData.reprojectedProbes[reprojectedIndex] = spawnedIndex;
    }
}

[shader("compute")]
[numthreads(32, 1, 1)]
void reorderSpawnedRays(uint3 coords : SV_DispatchThreadID) {
    if(probeData.counts[0].reprojectedProbes == 0) {
        return;
    }

    const uint emptyProbeIndex = coords.x;
    if(emptyProbeIndex >= probeData.counts[0].emptyProbes) {
        return;
    }

    // pick a reprojected probe to steal from
    const uint probeIndex = probeData.emptyProbes[emptyProbeIndex].load(MemoryOrder::SeqCst);
    const uint indexToSteal = emptyProbeIndex % probeData.counts[0].reprojectedProbes;

    // change which probe index the reprojected probe points to, and make it point to our empty probe
    uint stolenProbeIndex = probeData.reprojectedProbes[indexToSteal].load(MemoryOrder::SeqCst);

    probeData.spawnedProbes[stolenProbeIndex].exchange(probeIndex, MemoryOrder::SeqCst);
}

[shader("compute")]
[numthreads(32, 1, 1)]
void spawnRays(uint3 coords : SV_DispatchThreadID) {
    const uint spawnedProbeIndex = coords.x;

    if(spawnedProbeIndex >= probeData.spawnedProbes.getCount()) {
        return;
    }

    uint startRayIndex = 0;
    InterlockedAdd(probeData.counts[0].spawnedRays, MaxRaysPerProbe, /*out*/startRayIndex);
    const uint probeIndex = probeData.spawnedProbes[spawnedProbeIndex].load(MemoryOrder::AcquireRelease);
    probeData.rayDataStart[probeIndex] = startRayIndex;
    for(int i = 0; i < MaxRaysPerProbe; i++) {
        // TODO: decide direction of ray at this point
        probeData.rayData[i + startRayIndex].probeIndex = probeIndex;
        probeData.rayData[i + startRayIndex].radiance = float3(0);
        probeData.rayData[i + startRayIndex].direction = float3(0);
    }
}

// Shoot rays and accumulate radiance to the touched cells
[shader("compute")]
[numthreads(32, 1, 1)]
void traceRays(uint3 coords : SV_DispatchThreadID, uniform LightingPassUniforms push) {
    uint rayIndex = coords.x;

    if(rayIndex >= probeData.counts[0].spawnedRays) {
        return;
    }

    RayData* rayData = &probeData.rayData[rayIndex];
    const uint probeIndex = rayData.probeIndex;
    const int2 pixel = probeData.probes[probeIndex].bestPixel;
    const uint2 size = uint2(push.frameWidth, push.frameHeight);

    if(pixel.x >= size.x || pixel.y >= size.y) {
        return;
    }

    const float2 uv = float2(pixel) / size;
    const PixelInfo pixelInfo = PixelInfo(cameras.CurrentFrame(), gBufferInputs, uv);
    RandomSampler rng = RandomSampler(uv, push.frameWidth, push.frameHeight, rayIndex + push.frameCount);
    Raytracing::HardwareCapability capability = Raytracing::HardwareCapability(rtData);
    const float tMax = 50000.0f;

    const float3 brdf = M_INV_PIf;
    float3 radiance = lightData.lightArray.ambientColor;
    float3 from = pixelInfo.worldPosition;

    const float3x3 cboNormalView = transpose(inverse(float3x3(cameras.CurrentFrame().view)));
    const float3x3 inverseNormalView = inverse(cboNormalView);
    const float3 direction = inverseNormalView * pixelInfo.gBuffer.viewTBN * rng.nextCosineHemisphere(materialSystem);

    // avoid self shadowing
    from += direction * 0.01f;

    rayData->direction = direction;
    
    if(let intersection = capability.raytrace(materialSystem, uv, from, direction, tMax)) {
        PbrInputs pbr;
        pbr.alpha = intersection.computedMaterial.roughness*intersection.computedMaterial.roughness;
        pbr.metallic = intersection.computedMaterial.metallic;
        pbr.baseColor = intersection.computedMaterial.albedo.rgb;
        pbr.V = -direction;
        pbr.N = intersection.worldNormal;
        pbr.NdotV = abs(dot(pbr.N, pbr.V));

        float lightPDF = 1;

        // TODO: merge GIInputs with Key
        HashGrid::GIInputs giInputs;
        giInputs.cameraPosition = pixelInfo.cameraPos;
        giInputs.hitPosition = intersection.worldPosition;
        giInputs.surfaceNormal = intersection.worldNormal;
        giInputs.startOfRay = from;

        hashGridData.grids.currentFrame->jitter(rng, materialSystem, giInputs);
        float3 gi = hashGridData.grids.currentFrame->read(rng, giInputs);
        float3 directLighting = lightData.sampleLights(/*out*/ lightPDF, capability, pbr, 
            intersection.worldPosition, intersection.worldNormal, intersection.worldTangent, intersection.computedMaterial.metallic, intersection.computedMaterial.roughness);
        radiance += directLighting/*already contains BRDF*/ + brdf * (intersection.computedMaterial.emissive);

        bool wasNew = false;
        HashGrid::Key key;
        key.hitPosition = giInputs.hitPosition;
        key.direction = -direction;
        key.cameraPosition = giInputs.cameraPosition;
        key.rayLength = length(key.hitPosition - from);
        // TODO: reservoirs
        /*if(dot(radiance, radiance) > 0.001f)*/ { // don't accumulate poor samples
            if(let cellIndex = hashGridData.grids.currentFrame.insert(key, wasNew)) {
                if(wasNew) {
                    hashGridData.grids.currentFrame.writeToCell(cellIndex, key, 0, 0);
                }
                hashGridData.grids.currentFrame.mark(cellIndex, push.frameCount);
                hashGridData.grids.currentFrame.accumulate(cellIndex, key, radiance);
            }
            // TODO: Radiance feedback
        }

        radiance += brdf * gi;
    } else {
        // correct orientation for skybox cubemap
        constexpr float3x3 rot = float3x3(
            float3(1.0, 0.0, 0.0),
            float3(0.0, 0.0, -1.0),
            float3(0.0, 1.0, 0.0)
        );

        const float3 skyboxRGB = gBufferInputs.skybox3D.SampleGrad(rot * direction, float3(0), float3(0)).rgb;
        
        radiance += brdf * skyboxRGB;
    }

    rayData->radiance = radiance;
}

// Merge accumulated radiance and already present radiance in each cell
[shader("compute")]
[numthreads(32, 1, 1)]
void accumulateRadiance(uint cellIndex : SV_DispatchThreadID) {
    if(cellIndex >= HashGrid::TotalCellCount) {
        return;
    }

    HashGrid::Storage* grid = hashGridData.grids.currentFrame;
    grid->mergeRadiance(cellIndex);
}

[shader("compute")]
[numthreads(32, 1, 1)]
void accumulateProbes(uint probeIndex : SV_DispatchThreadID) {
    if(probeIndex >= probeData.probes.getCount()) {
        return;
    }

    const uint rayStart = probeData.rayDataStart[probeIndex].load();
    for(uint rayIndex = rayStart; rayIndex < rayStart + MaxRaysPerProbe; rayIndex++) {
        probeData.probes[probeIndex].accumulateSample(probeData.rayData[rayIndex]);
    }

    probeData.probes[probeIndex].finishAccumulateSamples();
}