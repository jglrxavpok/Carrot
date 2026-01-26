import modules.base;
import modules.gbuffer;
import modules.debug;
import modules.camera;
import modules.materials;
import modules.lighting;
import modules.compute_fullscreen;

ParameterBlock<GBufferInputs> gBufferInputs;

struct LightingInputs {
    Texture2D lighting;
    Texture2D ao;
    Texture2D visibilityBufferDebugs[MergeLightingOutput::VisibilityBufferLast - MergeLightingOutput::VisibilityBufferFirst + 1];
}
ParameterBlock<LightingInputs> lightingInputs;
ParameterBlock<DebugParams> debug;
ParameterBlock<Camera> cameras;
ParameterBlock<MaterialSystem> materialSystem;
ParameterBlock<Lighting> lightingData;

float3 computeLighting(float2 uv) {
    const float ao = lightingInputs.ao.Sample(gBufferInputs.linearSampler, uv).r;
    return lightingInputs.lighting.Sample(gBufferInputs.linearSampler, uv).rgb * ao * ao;
}

[shader("pixel")]
float4 pixel(uniform LightingPassUniforms push, in float2 uv) {
    const GBuffer gBuffer = gBufferInputs.read(uv);
    const PixelInfo pixelInfo = PixelInfo(cameras.CurrentFrame(), gBuffer);

    if(debug.mergeLightingOutput != MergeLightingOutput::Default) {
        return debugRendering(pixelInfo, uv, push);
    }

    float3 color;
    float distanceToCamera;
    if(pixelInfo.isSky) {
        const float4 viewSpaceDir = cameras.CurrentFrame().inverseNonJitteredProjection * float4(uv*2-1, 0, 1);
        const float3 worldViewDir = float3x3(cameras.CurrentFrame().inverseView) * viewSpaceDir.xyz;

        // correct orientation for skybox cubemap
        constexpr float3x3 rot = float3x3(
            float3(1.0, 0.0, 0.0),
            float3(0.0, 0.0, -1.0),
            float3(0.0, 1.0, 0.0)
        );

        const float3 skyboxRGB = gBufferInputs.skybox3D.SampleGrad(rot * worldViewDir, float3(0), float3(0)).rgb;
        color = skyboxRGB;
        distanceToCamera = 1.0f / 0.0f; // set distance to infinite to allow fog to apply
    } else {
        const float3 lighting = computeLighting(uv);
        color = gBuffer.albedo.rgb * lighting;
        color += gBuffer.emissiveColor;

        distanceToCamera = length(pixelInfo.worldPosition - pixelInfo.cameraPos);
    }

    // fog computation
    const float fogFactor = clamp((distanceToCamera - lightingData.lightArray.fogDistance) / lightingData.lightArray.fogDepth, 0, 1);
    color = lerp(color, lightingData.lightArray.fogColor, fogFactor);

    return float4(color, 1);
}

float4 debugRendering(in PixelInfo pixelInfo, float2 uv, in LightingPassUniforms push) {
    switch(debug.mergeLightingOutput) {
        case MergeLightingOutput::Albedo:
            return pixelInfo.gBuffer.albedo;

        case MergeLightingOutput::Position:
            return float4(pixelInfo.gBuffer.viewPosition, 1);

        case MergeLightingOutput::Normal:
            return float4(pixelInfo.gBuffer.viewTBN[2], 1);

        case MergeLightingOutput::Tangent:
            return float4(pixelInfo.gBuffer.viewTBN[0], 1);

        case MergeLightingOutput::Depth:
            return float4(pixelInfo.gBuffer.readDepth.rrr, 1);

        case MergeLightingOutput::MetallicRoughness:
            return float4(pixelInfo.gBuffer.metallicness, pixelInfo.gBuffer.roughness, 0, 1);

        case MergeLightingOutput::Emissive:
            return float4(pixelInfo.gBuffer.emissiveColor, 1);

        case MergeLightingOutput::Motion:
            const float x = uv.x * push.frameWidth;
            const float y = uv.y * push.frameHeight;
            const float2 p = float2(x, y);
            const float2 tileCenter = (floor(p / ARROW_TILE_SIZE) + 0.5) * ARROW_TILE_SIZE;

            const float2 fieldAtTileCenter = 100 * (-gBufferInputs.readMotionVector(tileCenter / float2(push.frameWidth, push.frameHeight)));
            const float arrowColor = 1-arrow(tileCenter, p, fieldAtTileCenter * ARROW_TILE_SIZE * 0.4);
            const float3 motionVectorColor = float3(abs(pixelInfo.gBuffer.motionVector.xy)*2, 0);
            return float4(motionVectorColor + arrowColor.rrr * pixelInfo.gBuffer.albedo.rgb, 1);

        case MergeLightingOutput::EntityID:
            return float4(pixelInfo.gBuffer.entityID) / 255.0f;

            // TODO: debug for reflections
        case MergeLightingOutput::Lighting:
            return float4(computeLighting(uv), 1);

    }

    if(debug.mergeLightingOutput >= MergeLightingOutput::VisibilityBufferFirst && debug.mergeLightingOutput <= MergeLightingOutput::VisibilityBufferLast) {
        int index = debug.mergeLightingOutput - MergeLightingOutput::VisibilityBufferFirst;
        return lightingInputs.visibilityBufferDebugs[index].Sample(gBufferInputs.linearSampler, uv);
    }
    return 0;
}