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
    float distanceToCamera; // used for fog
    float3 finalColor;
    if(!pixelInfo.isSky) { // check if not hitting the sky
        float3 lightContribution = float3(0);
        distanceToCamera = length(pixelInfo.gBuffer.viewPosition);

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
        const float4 viewSpaceDir = cameras.CurrentFrame().inverseNonJitteredProjection * float4(uv*2-1, 0, 1);
        const float3 worldViewDir = float3x3(cameras.CurrentFrame().inverseView) * viewSpaceDir.xyz;

        // correct orientation for skybox cubemap
        constexpr float3x3 rot = float3x3(
            float3(1.0, 0.0, 0.0),
            float3(0.0, 0.0, -1.0),
            float3(0.0, 1.0, 0.0)
        );

        const float3 skyboxRGB = gBufferInputs.skybox3D.SampleGrad(rot * worldViewDir, float3(0), float3(0)).rgb;
        
        distanceToCamera = 1.0f / 0.0f; // set distance to infinite to allow fog to apply
        finalColor = skyboxRGB;
    }

    // TODO: move out of lighting, albedo is multiplied with the fog color
    const float fogFactor = clamp(distanceToCamera - lighting.lightArray.fogDistance / lighting.lightArray.fogDepth, 0, 1);
    finalColor = lerp(finalColor, lighting.lightArray.fogColor, fogFactor);

    io.outDirectLightingImage.Store(coords, float4(finalColor, 1));
}