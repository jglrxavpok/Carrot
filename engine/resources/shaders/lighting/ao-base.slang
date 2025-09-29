import modules.lighting;
import modules.gbuffer;
import modules.camera;
import modules.materials;
import modules.base;
import modules.compute_fullscreen;
import modules.raytracing_capabilities;
import modules.rng;

ParameterBlock<GBufferInputs> gBufferInputs;
ParameterBlock<Camera> cameras;
ParameterBlock<Lighting> lighting;

struct Dummy{
    uint dummy;
}
ParameterBlock<Dummy> debugDummy;
ParameterBlock<MaterialSystem> materials;

struct IO {
    RWTexture2D<float4> outDirectLightingImage;
}
ParameterBlock<IO> io;

void computeAO(IRaytracingCapabilities config, uint2 coords, LightingPassUniforms push) {

    uint w, h;
    io.outDirectLightingImage.GetDimensions(w, h);

    if(coords.x >= w || coords.y >= h) {
        return;
    }

    const float2 uv = float2(coords.xy) / float2(w, h);

    PixelInfo pixelInfo = PixelInfo(cameras.CurrentFrame(), gBufferInputs, uv);
    float ao = 0.0f;
    if(!pixelInfo.isSky) { 
        RandomSampler rng = RandomSampler(uv, push.frameWidth, push.frameHeight, push.frameCount);
        float3 lightContribution = float3(0);

        const int sampleCount = config.getAOSampleCount();

        const float3 tangentCorrected = normalize(pixelInfo.worldTangent - dot(pixelInfo.worldTangent, pixelInfo.worldNormal) * pixelInfo.worldNormal);
        const float3 bitangent = cross(tangentCorrected, pixelInfo.worldNormal);
        const float3x3 worldTBN = float3x3(tangentCorrected, bitangent, pixelInfo.worldNormal);

        for(int sampleIndex = 0; sampleIndex < sampleCount; sampleIndex++) {
            float3 randomDirection = rng.nextCosineHemisphere(materials);
            float3 direction = worldTBN * randomDirection;
            
            const float3 offset = direction * 0.001f;
            const float3 rayVec = direction * 0.02f; // 2cm
            float sample = config.checkVisibility(pixelInfo.worldPosition + offset, pixelInfo.worldPosition + offset + rayVec) ? 1.0f : 0.0f;
            ao += sample;
        }
        ao /= sampleCount;
    } else {
        ao = 1.0f;
    }
        
    io.outDirectLightingImage.Store(coords, float4(ao.rrr, 1));
}