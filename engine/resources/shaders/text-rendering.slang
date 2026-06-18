import modules.base;
import modules.camera;
import modules.materials;
import modules.gbuffer;
import modules.viewport;
import modules.per_object;

[vk::binding(0, 0)]
ParameterBlock<MaterialSystem> materialSystem;

[vk::binding(0, 1)]
ParameterBlock<Camera> camera;

[vk::binding(0, 2)]
ParameterBlock<PerObjectDrawBuffers> objects;

[vk::binding(0, 3)]
ParameterBlock<Viewport> viewport;

[vk::binding(0, 4)]
StructuredBuffer<uint4> hb_gpu_atlas;

struct VSInput {
    // Per vertex
    vec3 position;

    // Per instance
    vec4 instanceColor;
    uvec4 uuid;
    mat4 instanceTransform;
}

struct VSOutput {
    vec4 hPos : SV_Position;
    vec2 uv;
    vec3 viewPos;
    nointerpolation vec4 instanceColor;
    nointerpolation mat4 modelview;
    nointerpolation int drawID;
    nointerpolation uint32_t glyphLoc;
}

[shader("vertex")]
VSOutput vertex(VSInput input, int drawID : SV_DrawIndex) {
    VSOutput o;

    o.drawID = drawID;
    o.uv = input.position.xy;
    const CameraBufferObject cbo = camera.CurrentFrame();
    const mat4 modelview = cbo.view * input.instanceTransform;
    const vec4 viewPosition = modelview * vec4(input.position, 1.0f);
    o.hPos = cbo.jitteredProjection * viewPosition;

    o.instanceColor = input.instanceColor;
    o.viewPos = viewPosition.xyz;
    o.modelview = modelview;

    return o;
}

[shader("pixel")]
EncodedGBuffer pixel(VSOutput input) {
    const ObjectDrawData instanceDrawData = objects.Load(NonUniformResourceIndex(input.drawID));
    const Material material = materialSystem.materials[NonUniformResourceIndex(instanceDrawData.materialIndex)];

    float color = materialSystem.SampleAlbedo(material, materialSystem.linearSampler, input.uv).r;
    if (color < 0.5f) {
        discard;
    }

    GBuffer gbuffer;

    gbuffer.albedo = vec4(1, 1, 1, 1) * input.instanceColor;
    gbuffer.viewPosition = input.viewPos;
    gbuffer.intProperty = IntProperties::RayTracedLighting;

    return EncodedGBuffer(gbuffer);
}

import modules.harfbuzz;

struct PushData {
    int16_t4 *gpuAtlas;
};

static int16_t4 *gpuAtlas;
export int4 hb_gpu_fetch(int offset) {
    return int4(gpuAtlas[offset]);
}

struct HBInput {
    float2 position;

    float2 normal; 

    float2 texcoord;

    float emPerPos;

    uint32_t glyphLoc;

    // Per instance
    vec4 instanceColor;
    uvec4 uuid;
    mat4 instanceTransform;
}

[shader("vertex")]
VSOutput vertex2(HBInput input, int drawID: SV_DrawIndex) {
    VSOutput o;

    o.drawID = drawID;

    const CameraBufferObject cbo = camera.CurrentFrame();
    const mat4 modelview = cbo.view * input.instanceTransform;
    const mat4 modelviewproj = cbo.jitteredProjection * modelview;

    float4 jac = vec4(input.emPerPos, 0.0, 0.0, -input.emPerPos);
    //harfbuzzDilateVertex(input.position, input.texcoord, input.normal, jac, modelviewproj, viewport.frameDimensions);
    float4 viewPos = modelview * float4(input.position.x, 0, input.position.y, 1);
    o.hPos = cbo.jitteredProjection * viewPos;
    o.uv = input.texcoord;
    o.viewPos = viewPos.xyz;
    o.glyphLoc = input.glyphLoc;
    o.instanceColor = input.instanceColor;

    return o;
}

[shader("pixel")]
EncodedGBuffer pixel2(VSOutput input, uniform int16_t4 *gpuAtlasAddress) {
    gpuAtlas = gpuAtlasAddress;
    GBuffer gbuffer;

    float coverage = 0;
    gbuffer.albedo = harfbuzzPaint(input.uv, input.glyphLoc, input.instanceColor, coverage);
    if (coverage < 0.95) {
        discard;
    }
    //gbuffer.albedo = lerp(gbuffer.albedo, float4(1, 1, 1, 1), 0.25f);
    gbuffer.viewPosition = input.viewPos;
    gbuffer.intProperty = IntProperties::RayTracedLighting;

    return EncodedGBuffer(gbuffer);
}