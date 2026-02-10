import modules.base;
import modules.gi;
import modules.materials;
import modules.lighting;
import modules.gbuffer;
import modules.camera;
import modules.raytracing;
import modules.rng;
import modules.compute_fullscreen;
import modules.hash;

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
    RWStructuredBuffer<uint, ScalarDataLayout> spawnedProbes;
    RWStructuredBuffer<uint, ScalarDataLayout> emptyProbes;
    RWStructuredBuffer<uint, ScalarDataLayout> reprojectedProbes;
    RWStructuredBuffer<uint, ScalarDataLayout> rayDataStart; // start into rayData, per probe
    RWStructuredBuffer<RayData, ScalarDataLayout> rayData;
}

ParameterBlock<HashGrid::Block> hashGridData;
ParameterBlock<Probes> probeData;
ParameterBlock<MaterialSystem> materialSystem;
ParameterBlock<Lighting> lightData;
ParameterBlock<GBufferInputs> gBufferInputs;
ParameterBlock<GBufferInputs> previousFrameGBufferInputs;
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
        const int2 probeLocation = reprojectedPixel / ScreenProbeSize;
        const uint reprojectedProbeIndex = probeLocation.x + probeLocation.y * probesPerWidth;
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
[numthreads(ScreenProbeSize, ScreenProbeSize, 1)]
void spawnScreenProbes(uint3 coords : SV_DispatchThreadID, uint localIndex : SV_GroupIndex, uint3 groupID : SV_GroupID, uniform LightingPassUniforms push) {
    // this will store the best matching pixel for this workgroup
    reprojectionScore = packHalf(65504.0f) << 16/*score*/ | 0xFFFFu /*lane*/;

    const uint probesPerWidth = (push.frameWidth + ScreenProbeSize-1) / ScreenProbeSize;
    const uint probeIndex = groupID.x + groupID.y * probesPerWidth;
    if(probeIndex >= probeData.probes.getCount()) {
        return;
    }
    int2 pixel = coords.xy;
    const float2 size = float2(push.frameDimensions);
    const float2 uv = (pixel + 0.5f) / size;

    const PixelInfo pixelInfo = PixelInfo(cameras.CurrentFrame(), gBufferInputs, uv);

    GroupMemoryBarrier();
    if(!pixelInfo.isSky) {
        // check if we can reuse a probe from the previous frame
        if(let reprojectedProbeIndex = getReprojectedProbeIndex(push.frameDimensions, uv, pixelInfo.gBuffer.motionVector.xy)) {
            const ScreenProbe reprojectedProbe = probeData.previousFrameProbes[reprojectedProbeIndex];
            const float3 dPos = reprojectedProbe.worldPos - pixelInfo.worldPosition;
            const float planeDist = abs(dot(dPos, pixelInfo.worldNormal));
            const float normalCheck = dot(pixelInfo.worldNormal, reprojectedProbe.normal);
            const float cellSize = HashGrid::giGetCellSize(pixelInfo.worldPosition, pixelInfo.cameraPos);
            if(planeDist < cellSize && normalCheck > 0.95) {
                const float dist = length(dPos) / cellSize;
                const uint probeScore = (packHalf(dist) << 16) | localIndex;
                InterlockedMin(reprojectionScore, probeScore);
            }
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
        probeData.emptyProbes[emptyIndex] = probeIndex;
    } else {
        const int2 screenTileStart = int2(groupID.xy * uint2(ScreenProbeSize));
        const uint localPixel = reprojectionScore & 0xFFFFu;
        const int2 posInTile = int2(localPixel % ScreenProbeSize, localPixel / ScreenProbeSize);
        const int2 reprojectedPixelPos = screenTileStart + posInTile;
        const float2 reprojectedUV = (float2(reprojectedPixelPos) + float2(0.5)) / size;
        const GBuffer currentGBuffer = gBufferInputs.read(reprojectedUV);
        const GBuffer reprojectedGBuffer = previousFrameGBufferInputs.read(reprojectedUV + currentGBuffer.motionVector.xy);

        const uint reprojectedProbeIndex = getReprojectedProbeIndex(push.frameDimensions, reprojectedUV, currentGBuffer.motionVector.xy).value;
        const ScreenProbe reprojectedProbe = probeData.previousFrameProbes[reprojectedProbeIndex];

        probeData.probes[probeIndex].reprojectRadiance(reprojectedProbe);

        const float3 worldPos = (cameras.CurrentFrame().inverseView * float4(currentGBuffer.viewPosition, 1)).xyz;
        const float3 worldNormal = transpose(inverse(float3x3(cameras.CurrentFrame().inverseView))) * currentGBuffer.viewTBN[2];
        probeData.probes[probeIndex].worldPos = worldPos;
        probeData.probes[probeIndex].normal = worldNormal;
        

        uint reprojectedIndex;
        InterlockedAdd(probeData.counts[0].reprojectedProbes, 1, reprojectedIndex);
        probeData.reprojectedProbes[reprojectedIndex] = spawnedIndex;
    }
    // TODO: better jitter pattern
    // jitter inside tile
    pixel.x += push.frameCount % ScreenProbeSize;
    pixel.y += (push.frameCount / ScreenProbeSize) % ScreenProbeSize;
    probeData.probes[probeIndex].bestPixel = pixel;
}

[shader("compute")]
[numthreads(32, 1, 1)]
void reorderSpawnedRays(uint3 coords : SV_DispatchThreadID) {
    const uint emptyProbeIndex = coords.x;

    if(probeData.counts[0].reprojectedProbes == 0) {
        return;
    }

    if(emptyProbeIndex >= probeData.counts[0].emptyProbes) {
        return;
    }
    
    // pick a reprojected probe to steal from
    const uint probeIndex = probeData.emptyProbes[emptyProbeIndex];
    const int M = 1;
    for(int i = 0; i < M; i++) {
        const uint indexToSteal = pcg_hash(emptyProbeIndex*M+i) % probeData.counts[0].reprojectedProbes;

        // change which probe index the reprojected probe points to, and make it point to our empty probe
        uint stolenSpawnedIndex = probeData.reprojectedProbes[indexToSteal];

        probeData.spawnedProbes[stolenSpawnedIndex] = probeIndex;
    }
}

// theta in [0;2pi] and phi in [0;pi/2]
float3 getHemisphereDirection(float theta, float phi) {
    const float x = sin(theta) * cos(phi);
    const float y = sin(theta) * sin(phi);
    const float z = cos(theta);
    return (float3(x, y, z));
}

static const int GuidingDirectionsAlongLatitude = 8;
static const int GuidingDirectionsAlongLongitude = 4;
static const int GuidingDirectionCount = GuidingDirectionsAlongLatitude*GuidingDirectionsAlongLongitude;

// Based on an index in [0;GuidingDirectionCount), gives a "guiding" direction
//  This is a direction that will be used to send more rays in, because we previously noticed that it was interesting (ie had a lot of radiance)
float3 getGuidingDirection(int index) {
    const int latitudeIndex = index % GuidingDirectionsAlongLatitude;
    const int longitudeIndex = index / GuidingDirectionsAlongLatitude;
    const float theta = (longitudeIndex+0.5f) * M_PIf * 2 / GuidingDirectionsAlongLongitude;
    const float phi = (latitudeIndex+0.5f) * M_PIf /2 / GuidingDirectionsAlongLatitude;
    return getHemisphereDirection(theta, phi);
}

// u and v are random variables in [0;1]
float3 sampleInDirection(int index, float u, float v) {
    const int latitudeIndex = index % GuidingDirectionsAlongLatitude;
    const int longitudeIndex = index / GuidingDirectionsAlongLatitude;
    const float theta = (longitudeIndex+u) * M_PIf * 2 / GuidingDirectionsAlongLongitude;
    const float phi = (latitudeIndex+v) * M_PIf/2 / GuidingDirectionsAlongLatitude;
    return getHemisphereDirection(theta, phi);
}

float luminance(float3 rgb)
{
    return dot(rgb, float3(0.2126F, 0.7152F, 0.0722F));
}

[shader("compute")]
[numthreads(32, 1, 1)]
void spawnRays(uint3 coords : SV_DispatchThreadID, uniform LightingPassUniforms push) {
    const uint spawnedProbeIndex = coords.x;

    if(spawnedProbeIndex >= probeData.spawnedProbes.getCount()) {
        return;
    }

    const uint probeIndex = probeData.spawnedProbes[spawnedProbeIndex];
    const int2 pixel = probeData.probes[probeIndex].bestPixel;
    const uint2 size = uint2(push.frameWidth, push.frameHeight);

    if(pixel.x >= size.x || pixel.y >= size.y) {
        return;
    }

    const float2 uv = float2(pixel + 0.5) / size;
    const PixelInfo pixelInfo = PixelInfo(cameras.CurrentFrame(), gBufferInputs, uv);

    if(pixelInfo.isSky) {
        return;
    }

    uint startRayIndex = 0;
    InterlockedAdd(probeData.counts[0].spawnedRays, MaxRaysPerProbe, /*out*/startRayIndex);

    RandomSampler rng = RandomSampler(uv, push.frameWidth, push.frameHeight, pcg_hash(spawnedProbeIndex + push.frameCount));

    float3 radiance = lightData.lightArray.ambientColor;
    float3 from = pixelInfo.worldPosition;

    const float3x3 cboNormalView = transpose(inverse(float3x3(cameras.CurrentFrame().view)));
    const float3x3 inverseNormalView = inverse(cboNormalView);
    const float3 cameraPosition = pixelInfo.cameraPos;

    float cdf[GuidingDirectionCount] = {0};
    float sum = 0.0f;
    for(int guidingDirectionIndex = 0; guidingDirectionIndex < GuidingDirectionCount; guidingDirectionIndex++) {
        const float3 guidingDirection = inverseNormalView * pixelInfo.gBuffer.viewTBN * getGuidingDirection(guidingDirectionIndex);

        const float3 radiance = probeData.probes[probeIndex].evaluateCosineLobe(guidingDirection);
        const float contribution = luminance(radiance);
        cdf[guidingDirectionIndex] = sum;
        sum += contribution;
    }

    for(int i = 0; i < MaxRaysPerProbe; i++) {
        const bool goInRandomDirection = true;//rng.next(materialSystem) < 0.05f;

        float3 direction;
        if(!goInRandomDirection && probeData.probes[probeIndex].getSampleCount() > 10 && sum > 0.01f) {
            const float r = rng.next(materialSystem);
            const float u = rng.next(materialSystem);
            const float v = rng.next(materialSystem);

            int selectedIndex = 0;
            for(int guidingDirectionIndex = 0; guidingDirectionIndex < GuidingDirectionCount; guidingDirectionIndex++) {
                if(cdf[guidingDirectionIndex] / sum > r) {
                    break;
                }
                selectedIndex++;
            }
            direction = inverseNormalView * pixelInfo.gBuffer.viewTBN * sampleInDirection(selectedIndex, u, v);
        } else {
            direction = inverseNormalView * pixelInfo.gBuffer.viewTBN * rng.nextCosineHemisphere(materialSystem);
        }
        
        probeData.rayData[i + startRayIndex].probeIndex = probeIndex;
        probeData.rayData[i + startRayIndex].radiance = float3(0);
        probeData.rayData[i + startRayIndex].direction = direction;
        probeData.rayData[i + startRayIndex].from = from + direction * 0.01f; // avoid self shadowing
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
    const uint probeIndex = rayData->probeIndex;
    const int2 pixel = probeData.probes[probeIndex].bestPixel;
    const uint2 size = uint2(push.frameWidth, push.frameHeight);

    if(pixel.x >= size.x || pixel.y >= size.y) {
        return;
    }

    const float2 uv = float2(pixel + 0.5f) / size;
    const PixelInfo pixelInfo = PixelInfo(cameras.CurrentFrame(), gBufferInputs, uv);

    if(pixelInfo.isSky) {
        return;
    }

    RandomSampler rng = RandomSampler(uv, push.frameWidth, push.frameHeight, rayIndex + push.frameCount);
    Raytracing::HardwareCapability capability = Raytracing::HardwareCapability(rtData);
    const float tMax = 50000.0f;

    const float3 brdf = M_INV_PIf;
    float3 radiance = lightData.lightArray.ambientColor;

    const float3 direction = rayData->direction;
    const float3 from = rayData->from;
    
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

        // TODO: isn't this missing some information about distance/angle to light and intersection?
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
        // TODO: remove?
        if(luminance(radiance) > 0.001f) { // don't accumulate poor samples
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
void accumulateProbesPart1(uint rayIndex : SV_DispatchThreadID) {
    if(rayIndex >= probeData.counts[0].spawnedRays) {
        return;
    }

    RayData rayData = probeData.rayData[rayIndex];
    probeData.probes[rayData.probeIndex].accumulateSample(rayData);    
}

[shader("compute")]
[numthreads(32, 1, 1)]
void accumulateProbesPart2(uint probeIndex : SV_DispatchThreadID) {
    if(probeIndex >= probeData.probes.getCount()) {
        return;
    }

    probeData.probes[probeIndex].finishAccumulateSamples();
}