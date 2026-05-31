import modules.base;
import modules.camera;
import modules.gbuffer;
import modules.per_object;
import modules.materials;
import modules.viewport;
import modules.vertex_buffers;

ParameterBlock<Camera> camera;
ParameterBlock<MaterialSystem> materialSystem;
ParameterBlock<PerObjectDrawBuffers> objects;
ParameterBlock<Viewport> viewport;

struct VertexInput {
    float3 position;
    InstanceData instance;
}

struct VertexOutput {
    float2 uv;
    float4 instanceColor;
    float3 viewPosition;
    uint4 uuid;
    float3 previousFrameViewPosition;
    nointerpolation float4x4 modelview;
    nointerpolation int drawID;

    float4 svPos : SV_Position;
}

struct TexRegion {
    float2 center;
    float2 halfSize;
}

[shader("vertex")]
VertexOutput vertex(in VertexInput input, int drawID: SV_DrawIndex, uniform TexRegion region) {
    VertexOutput out;
    out.drawID = drawID;
    out.uv = (input.position.xy + float2(0.5)) * (region.halfSize * 2) + region.center - region.halfSize;
    out.uv.y *= -1;
    const float4x4 modelview = camera.CurrentFrame().view * input.instance.transform;
    const float4 viewPosition = modelview * float4(input.position, 1);
    const float4x4 prevFrameModelview = camera.CurrentFrame().view * input.instance.lastFrameTransform;
    const float4 previousFrameViewPosition = prevFrameModelview * float4(input.position, 1);
    out.previousFrameViewPosition = (camera.PreviousFrame().jitteredProjection * float4(previousFrameViewPosition.xyz / previousFrameViewPosition.w, 1)).xyw;

    out.svPos = camera.CurrentFrame().jitteredProjection * viewPosition;
    out.instanceColor = input.instance.color;
    out.viewPosition = viewPosition.xyz;
    out.uuid = input.instance.uuid;
    out.modelview = modelview;

    return out;
}

[shader("pixel")]
EncodedGBuffer pixel(in VertexOutput input, float4 fragCoords: SV_Position) {
    GBuffer result;

    const ObjectDrawData object = objects.Load(NonUniformResourceIndex(input.drawID));
    const Material material = materialSystem.materials[NonUniformResourceIndex(object.materialIndex)];
    const MaterialOutput output = material.Compute(materialSystem, materialSystem.linearSampler, input.uv);

    vec4 albedo = output.albedo;
    albedo *= input.instanceColor;
    if (albedo.a < 0.01) {
        discard;
    }

    if (albedo.a < 1.0) {
        // TODO: animate dithering? -> need smarter AA
        if (materialSystem.Dither(uvec2(fragCoords.xy)) >= albedo.a) {
            discard;
        }
    }
    albedo.a = 1; // alpha done via dithering

    result.albedo = albedo + vec4(result.emissiveColor, 0);
    result.emissiveColor = output.emissive;

    const mat3 toView = transpose(inverse(float3x3(input.modelview)));
    result.viewTBN = toView;
    result.metallicness = output.metallic;
    result.roughness = output.roughness;
    result.entityID = input.uuid;
    result.viewPosition = input.viewPosition;
    result.intProperty = IntProperties::RayTracedLighting;

    vec3 previousClipPos = input.previousFrameViewPosition;
    previousClipPos.xy /= previousClipPos.z;

    previousClipPos.xy = previousClipPos.xy * float2(0.5f, 0.5f) + 0.5f;
    previousClipPos.xy *= float2(viewport.frameDimensions);
    result.motionVector.xy = previousClipPos.xy - input.svPos.xy;
    result.motionVector.xy /= float2(viewport.frameDimensions);
    result.motionVector.xy += (camera.CurrentFrame().jitter + camera.PreviousFrame().jitter) * 0.5f;
 
    return EncodedGBuffer(result);
}