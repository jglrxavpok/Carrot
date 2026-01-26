import modules.lighting;
import modules.gbuffer;
import modules.camera;
import modules.materials;
import modules.base;
import modules.compute_fullscreen;
import modules.raytracing_capabilities;
import modules.debug;

ParameterBlock<GBufferInputs> gBufferInputs;
ParameterBlock<Camera> cameras;
ParameterBlock<Lighting> lighting;
ParameterBlock<DebugParams> debugDummy;
ParameterBlock<MaterialSystem> materials;

struct IO {
    RWTexture2D<float4> outDirectLightingImage;
}
ParameterBlock<IO> io;

void computeDirectLighting(IRaytracingCapabilities config, uint2 coords, LightingPassUniforms push) {
    uint w, h;
    io.outDirectLightingImage.GetDimensions(w, h);

    if(coords.x >= w || coords.y >= h) {
        return;
    }

    const float2 uv = float2(coords.xy) / float2(w, h);

    PixelInfo pixelInfo = PixelInfo(cameras.CurrentFrame(), gBufferInputs, uv);
    float3 finalColor;
    if(!pixelInfo.isSky) { // check if not hitting the sky
        float3 lightContribution = float3(0);

        const int sampleCount = config.getLightSampleCount();
        PbrInputs pbr;
        pbr.alpha = pixelInfo.gBuffer.roughness*pixelInfo.gBuffer.roughness;
        pbr.metallic = pixelInfo.gBuffer.metallicness;
        pbr.baseColor = pixelInfo.gBuffer.albedo.rgb;
        pbr.V = -normalize(pixelInfo.worldPosition-pixelInfo.cameraPos);
        pbr.N = pixelInfo.worldNormal;
        pbr.NdotV = abs(dot(pbr.N, pbr.V));
        for(int sampleIndex = 0; sampleIndex < sampleCount; sampleIndex++) {
            float lightPDF = 0.0f;
            float3 sample = lighting.sampleLights(lightPDF, config, pbr, 
                pixelInfo.worldPosition, pixelInfo.worldNormal, pixelInfo.worldTangent, 
                pixelInfo.gBuffer.metallicness, pixelInfo.gBuffer.roughness);
            lightContribution += sample * lightPDF;
        }
        lightContribution /= sampleCount;

        lightContribution += lighting.lightArray.ambientColor;
        lightContribution += pixelInfo.gBuffer.emissiveColor;

        finalColor = lightContribution;
    } else {
        finalColor = vec3(0.0);
    }

    io.outDirectLightingImage.Store(coords, float4(finalColor, 1));
}