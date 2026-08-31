import glsl;
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

static const int CacheDownscaleFactor = 2;
static const int MaxBounces = 4;
static const float SkyboxIntensity = 5;

struct IO {
    RWTexture2D<float4> output;
}

RWStructuredBuffer<HashGrid::Storage> hashGridData;
ParameterBlock<IO> io;
ParameterBlock<MaterialSystem> materialSystem;
ParameterBlock<Lighting> lightData;
ParameterBlock<GBufferInputs> gBufferInputs;
ParameterBlock<GBufferInputs> previousFrameGBufferInputs;
ParameterBlock<Camera> cameras;
ParameterBlock<Raytracing::Data> rtData;

struct DecayPushConstant {
    uint maxCellIndex;
    uint frameCount;
}

#define GetHashGrid() hashGridData[0]

[shader("compute")]
[numthreads(32, 1, 1)]
void decayCells(uint cellIndex: SV_DispatchThreadID, uniform DecayPushConstant push) {
    if(cellIndex >= push.maxCellIndex) {
        return;
    }

    if (GetHashGrid().isOutdated(cellIndex, push.frameCount)) {
        GetHashGrid().clearCell(cellIndex);
    }
}

[shader("compute")]
[numthreads(8, 4, 1)]
void fillSurfaceCache(uint2 coords: SV_DispatchThreadID, uniform LightingPassUniforms push) {
    const float2 offsetInTile = r2Sequence(push.frameCount) * CacheDownscaleFactor;
    const float2 uv = float2(coords * CacheDownscaleFactor + offsetInTile + 0.5f) / push.frameDimensions;
    const PixelInfo pixelInfo = PixelInfo(cameras.CurrentFrame(), gBufferInputs, uv);

    if (pixelInfo.isSky) {
        return;
    }

    const float3x3 cboNormalView = transpose(inverse(float3x3(cameras.CurrentFrame().view)));
    const float3x3 inverseNormalView = inverse(cboNormalView);

    Raytracing::HardwareCapability capability = Raytracing::HardwareCapability(rtData);
    const float tMax = 50000.0f;
    const float3 lambertianBRDF = M_INV_PIf;

    struct BounceData {
        Optional<uint> gridCellIndex = none;
        float3 directLight;
        float3 normal;
        float3 directionToNext;
        float pdfOfNextDirection = 1;
        float3 R;
    };

    PbrInputs pbr;
    BounceData bounces[MaxBounces+1];
    int bounceIndex = 0;

    float3 startOfRay = pixelInfo.worldPosition;
    float3 startingNormal = pixelInfo.worldNormal;
    float3 startingTangent = pixelInfo.worldTangent;

    float3 startingAlbedo = pixelInfo.gBuffer.albedo.rgb;
    float3 startingEmissive = pixelInfo.gBuffer.emissiveColor;
    float startingRoughness = pixelInfo.gBuffer.roughness;
    float startingMetallic = pixelInfo.gBuffer.metallicness;

    RandomSampler rng = RandomSampler(uv, push.frameWidth, push.frameHeight, push.frameCount);

    float directionPDF = 1;
    float3 rayDirection = rng.nextCosineHemisphere(materialSystem, startingNormal, normalize(startOfRay - pixelInfo.cameraPos), directionPDF);
    float3 previousPos = pixelInfo.cameraPos;
    bool hitSky = false;

    // forward trace: collect direct lighting at each bounce
    for (; bounceIndex < MaxBounces; bounceIndex++) {
        HashGrid::Key key;
        key.hitPosition = startOfRay;
        float3 dPos = previousPos - startOfRay;
        float l = length(dPos);
        key.direction = startingNormal;//dPos / l;
        key.cameraPosition = pixelInfo.cameraPos;
        key.rayLength = l;
        bool wasNew;

        bounces[bounceIndex].gridCellIndex = GetHashGrid().insert(key, wasNew);
        if (wasNew) {
            GetHashGrid().writeToCell(bounces[bounceIndex].gridCellIndex.value, key, float3(0), 0);
        }

        pbr.alpha = startingRoughness * startingRoughness;
        pbr.metallic = startingMetallic;
        pbr.baseColor = startingAlbedo;
        pbr.V = dPos / l;
        pbr.N = startingNormal;
        pbr.NdotV = abs(dot(pbr.N, pbr.V));

        float lightPDF = 1;
        float3 directLighting = lightData.sampleLights(lightPDF, capability, pbr, startOfRay, startingNormal, startingTangent, startingMetallic, startingRoughness);
        directLighting /= lightPDF;

        float emissivePDF = 1;
        float3 emissiveLighting = lightData.sampleEmissives(emissivePDF, rng, materialSystem, capability, pbr, startOfRay, startingNormal);
        directLighting += emissiveLighting / emissivePDF;

        directLighting += startingEmissive * 50;

        bounces[bounceIndex].R = startingAlbedo;
        bounces[bounceIndex].directLight = directLighting;
        bounces[bounceIndex].pdfOfNextDirection = directionPDF;
        bounces[bounceIndex].directionToNext = rayDirection;
        bounces[bounceIndex].normal = startingNormal;

        if (let intersection = capability.raytrace(materialSystem, uv, startOfRay + rayDirection * 0.01f, rayDirection, tMax)) {
            float3 worldBitangent = cross(intersection.worldNormal, intersection.worldTangent);
            float3x3 intersectionTBN = float3x3(intersection.worldTangent, worldBitangent, intersection.worldNormal);
            previousPos = startOfRay;
            startOfRay = intersection.worldPosition;
            // compute direction and store pdf of this direction

            float3 wo = -rayDirection;
            rayDirection = rng.nextCosineHemisphere(materialSystem, intersection.worldNormal, wo, directionPDF);
            startingRoughness = intersection.computedMaterial.roughness;
            startingMetallic = intersection.computedMaterial.metallic;
            startingAlbedo = intersection.computedMaterial.albedo.rgb;
            startingNormal = intersection.worldNormal;
            startingTangent = intersection.worldTangent;
            startingEmissive = intersection.computedMaterial.emissive;
        } else {
            // correct orientation for skybox cubemap
            constexpr float3x3 rot = float3x3(
                float3(1.0, 0.0, 0.0),
                float3(0.0, 0.0, -1.0),
                float3(0.0, 1.0, 0.0)
            );

            const float3 skyboxRGB = gBufferInputs.skybox3D.SampleGrad(rot * rayDirection, float3(0), float3(0)).rgb;

            bounces[bounceIndex].directLight += skyboxRGB * SkyboxIntensity * dot(startingNormal, rayDirection);
            hitSky = true;
            break;
        }
    }

    if (!hitSky) {
        // if we do hit the sky, we break out of the loop above, meaning we do not increment bounceIndex
#if 1
        bounceIndex--;
#else
        // sample cache for last bounce
        HashGrid::Key key;
        key.hitPosition = startOfRay;
        float3 dPos = previousPos - startOfRay;
        float l = length(dPos);
        key.direction = startingNormal; // dPos / l;
        key.cameraPosition = pixelInfo.cameraPos;
        key.rayLength = l;

    
        bounces[bounceIndex].directLight = GetHashGrid().read2(key);

        bounces[bounceIndex].R = startingAlbedo;
        bounces[bounceIndex].pdfOfNextDirection = directionPDF;
        bounces[bounceIndex].directionToNext = rayDirection;
        bounces[bounceIndex].normal = startingNormal;
#endif
    }

    uint lastBounceIndex = bounceIndex;
    for (; bounceIndex >= 0; bounceIndex--) {
        // rebuild path forward
        float3 L = lightData.lightArray.ambientColor;
        float3 beta = 1;

        for (int innerBounce = bounceIndex; innerBounce <= lastBounceIndex; innerBounce++) {
            L += beta * bounces[innerBounce].directLight;

            const float3 normal = bounces[innerBounce].normal;
            const float3 wi = bounces[innerBounce].directionToNext;
            const float pdf = bounces[innerBounce].pdfOfNextDirection;
            if (pdf <= 10e-6f) {
                break;
            }
            float3 f = bounces[innerBounce].R * lambertianBRDF; // TODO: proper BRDF?
            if (dot(f, f) == 0.0f) {
                break;
            }
            beta *= f * abs(dot(wi, normal)) / pdf;
        }

        if (let cellIndex = bounces[bounceIndex].gridCellIndex) {
            GetHashGrid().mark(cellIndex, push.frameCount);
            GetHashGrid().accumulate2(cellIndex, L);
        }
    }
}

[shader("compute")]
[numthreads(8, 8, 1)]
void computeGI(uint3 coords: SV_DispatchThreadID, uniform LightingPassUniforms push) {
    const float2 uv = float2(coords.xy + 0.5f) / push.frameDimensions;
    const PixelInfo pixelInfo = PixelInfo(cameras.CurrentFrame(), gBufferInputs, uv);

    if (pixelInfo.isSky) {
        return;
    }

    RandomSampler rng = RandomSampler(uv, push.frameWidth, push.frameHeight, push.frameCount);

    HashGrid::Key key;
    const float3 dPos = pixelInfo.cameraPos - pixelInfo.worldPosition;
    float l = length(dPos);
    key.hitPosition = pixelInfo.worldPosition;
    key.direction = pixelInfo.worldNormal;//dPos / l;
    key.cameraPosition = pixelInfo.cameraPos;
    key.rayLength = l;

    HashGrid::GIInputs giInputs;
    giInputs.cameraPosition = pixelInfo.cameraPos;
    giInputs.hitPosition = pixelInfo.worldPosition;
    giInputs.surfaceNormal = pixelInfo.worldNormal;
    giInputs.startOfRay = pixelInfo.cameraPos;
    GetHashGrid().jitter(rng, materialSystem, giInputs);

    float3 radiance = GetHashGrid().read(rng, giInputs);

    io.output.Store(coords.xy, float4(radiance, 1));
}

// Merge accumulated radiance and already present radiance in each cell
[shader("compute")]
[numthreads(32, 1, 1)]
void accumulateRadiance(uint cellIndex : SV_DispatchThreadID) {
    if(cellIndex >= HashGrid::TotalCellCount) {
        return;
    }

    GetHashGrid().mergeRadiance(cellIndex);
}
