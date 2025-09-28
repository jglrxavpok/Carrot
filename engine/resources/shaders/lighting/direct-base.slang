import modules.lighting;
import modules.gbuffer;
import modules.camera;
import modules.materials;
import modules.base;

interface IConfiguration {
    uint getSampleCount();

    bool checkVisibility(float3 from, float3 to);
}

extern struct Configuration : IConfiguration;

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

float3 sampleLights(out float lightPDF, in Configuration config, PbrInputs pbr, float3 worldPos, float3 normal, float3 tangent, float metallic, float roughness) {
    float3 contribution = 0;
    lightPDF = 1.0f;
    for(uint i = 0; i < lighting.activeLights.count; i++) {
        uint lightIndex = lighting.activeLights.indices[i];
        
        const Light l = lighting.lightArray.lights[lightIndex];
        
        if(!l.isEnabled()) {
            continue;
        }

        const float3 L = l.getLightPositionRelativeTo(worldPos);
        const float visibility = float(config.checkVisibility(worldPos, worldPos+L));
        float3 singleLightContribution = l.computeContribution(worldPos, normal) /* cos term already in computeLightContribution */;
        singleLightContribution *= l.intensity;

        pbr.L = normalize(L);
        pbr.H = normalize(pbr.L + pbr.V);

        const float NdotL = dot(pbr.N, pbr.L);
        if(NdotL <= 0.0) {
            continue;
        }

        pbr.NdotH = dot(pbr.N, pbr.H);
        pbr.NdotL = NdotL;
        pbr.HdotL = dot(pbr.H, pbr.L);
        pbr.HdotV = dot(pbr.H, pbr.V);

        const float3 brdf = glTF_BRDF_WithoutImportanceSampling(pbr);
        if(isnan(brdf.x)) {
            return float3(10, 0, 0);
        }

        contribution += brdf * visibility * singleLightContribution;
    }
    return contribution;
}

void computeDirectLighting(uint2 coords, LightingPassUniforms push) {
    Configuration config;

    uint w, h;
    io.outDirectLightingImage.GetDimensions(w, h);

    if(coords.x >= w || coords.y >= h) {
        return;
    }

    const float2 uv = float2(coords.xy) / float2(w, h);

    float depth = gBufferInputs.depth.Sample(gBufferInputs.gNearestSampler, uv).r;
    float distanceToCamera; // used for fog
    float3 finalColor;
    if(depth < 1.0) { // check if not hitting the sky
        float3 lightContribution = float3(0);

        const float3 cameraPos = (cameras.CurrentFrame().inverseView * float4(0, 0, 0, 1)).xyz;
        const GBuffer gbuffer = gBufferInputs.read(uv);
        const float4 hWorldPos = cameras.CurrentFrame().inverseView * float4(gbuffer.viewPosition, 1);
        const float3 worldPos = hWorldPos.xyz / hWorldPos.w;

        const float3x3 cboNormalView = transpose(inverse(float3x3(cameras.CurrentFrame().view)));
        const float3x3 inverseNormalView = inverse(cboNormalView);
        const float3 normal = inverseNormalView * gbuffer.viewTBN[2];
        const float3 tangent = inverseNormalView * gbuffer.viewTBN[0];
        
        const int sampleCount = config.getSampleCount();
        PbrInputs pbr;
        pbr.alpha = gbuffer.roughness*gbuffer.roughness;
        pbr.metallic = gbuffer.metallicness;
        pbr.baseColor = gbuffer.albedo.rgb;
        pbr.V = -normalize(worldPos-cameraPos);
        pbr.N = normal;
        pbr.NdotV = abs(dot(pbr.N, pbr.V));
        for(int sampleIndex = 0; sampleIndex < sampleCount; sampleIndex++) {
            float lightPDF = 0.0f;
            float3 sample = sampleLights(lightPDF, config, pbr, worldPos, normal, tangent, gbuffer.metallicness, gbuffer.roughness);
            lightContribution += sample * lightPDF;
        }
        lightContribution /= sampleCount;

        lightContribution += lighting.lightArray.ambientColor;
        lightContribution += gbuffer.emissiveColor;

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

        const float3 skyboxRGB = gBufferInputs.skybox3D.Sample(rot * worldViewDir).rgb;
        
        distanceToCamera = 1.0f / 0.0f; // set distance to infinite to allow fog to apply
        finalColor = skyboxRGB;
    }

    const float fogFactor = clamp(distanceToCamera - lighting.lightArray.fogDistance / lighting.lightArray.fogDepth, 0, 1);
    finalColor = lerp(finalColor, lighting.lightArray.fogColor, fogFactor);

    io.outDirectLightingImage.Store(coords, float4(finalColor, 1));
}