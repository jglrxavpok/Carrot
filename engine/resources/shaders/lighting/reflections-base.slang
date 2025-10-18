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
ParameterBlock<DebugParams> debug;
ParameterBlock<MaterialSystem> materials;

struct IO {
    RWTexture2D<float4> outImage;
    RWTexture2D<float4> firstBounceViewPosition;
    RWTexture2D<float4> firstBounceViewNormalsTangents;
}
ParameterBlock<IO> io;

void computeReflections(IRaytracingCapabilities config, uint2 coords, LightingPassUniforms push) {
    uint w, h;
    io.outImage.GetDimensions(w, h);

    if(coords.x >= w || coords.y >= h) {
        return;
    }
    

    const float2 uv = float2(coords.xy) / float2(w, h);

    PixelInfo pixelInfo = PixelInfo(cameras.CurrentFrame(), gBufferInputs, uv);
    if(pixelInfo.gBuffer.metallicness < 0.01) {
        return;
    }
    const float3 viewDir = normalize(pixelInfo.worldPosition - pixelInfo.cameraPos);
    const float3 reflectedDir = reflect(viewDir, pixelInfo.worldNormal);

    float3 finalColor;
    float3 firstBounceViewPos = float3(0);
    float3 firstBounceNormal = float3(0);
    float3 firstBounceTangent = float3(0);
    constexpr float MaxDistance = 500.0f;
    Optional<SurfaceIntersection> potentialIntersection = config.raytrace(materials, uv, pixelInfo.worldPosition, reflectedDir, MaxDistance);
    if(let intersection = potentialIntersection) {
        float lightPDF = 1;
        PbrInputs pbr;
        pbr.alpha = intersection.computedMaterial.roughness * intersection.computedMaterial.roughness;
        pbr.baseColor = intersection.computedMaterial.albedo.rgb;
        pbr.metallic = intersection.computedMaterial.metallic;
        pbr.V = -reflectedDir;
        pbr.N = -intersection.worldNormal; // I am not sure why this is necessary
        pbr.NdotV = abs(dot(pbr.V, pbr.N));

        const float3 lightingAtPoint = lighting.sampleLights(/*out*/lightPDF, config, pbr, 
            intersection.worldPosition, 
            pbr.N, 
            intersection.worldTangent, 
            intersection.computedMaterial.metallic,
            intersection.computedMaterial.roughness);
        const float3 gi = 0; // TODO
        const float ao = 1; // TODO

        firstBounceViewPos = (cameras.CurrentFrame().view * float4(intersection.worldPosition, 1)).xyz;
        const float3x3 cboNormalView = transpose(inverse(float3x3(cameras.CurrentFrame().view)));
        firstBounceNormal = cboNormalView * intersection.worldNormal;
        firstBounceTangent = cboNormalView * intersection.worldTangent;
        finalColor = lighting.lightArray.ambientColor + lightingAtPoint *ao*ao*intersection.computedMaterial.albedo.rgb + intersection.computedMaterial.emissive + gi;
    } else { // hit the sky
        // correct orientation for skybox cubemap
        constexpr float3x3 rot = float3x3(
            float3(1.0, 0.0, 0.0),
            float3(0.0, 0.0, -1.0),
            float3(0.0, 1.0, 0.0)
        );

        const float3 skyboxRGB = gBufferInputs.skybox3D.SampleGrad(rot * reflectedDir, float3(0), float3(0)).rgb;
        
        finalColor = skyboxRGB;
    }

    io.outImage.Store(coords, float4(finalColor, 1));
    io.firstBounceViewPosition.Store(coords, float4(firstBounceViewPos, 1));

    bool nSign = false;
    bool tSign = false;
    bool bSign = false;
    float4 compressedViewNormalTangent;
    compressTBN(float3x3(firstBounceTangent, 0, firstBounceNormal), compressedViewNormalTangent, nSign, tSign, bSign);
    io.firstBounceViewNormalsTangents.Store(coords, compressedViewNormalTangent);
}