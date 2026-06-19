import modules.base;
import modules.camera;
import modules.materials;
import modules.gbuffer;
import modules.viewport;
import modules.per_object;
import modules.harfbuzz;

[vk::binding(0, 0)]
ParameterBlock<Camera> camera;

[vk::binding(0, 1)]
ParameterBlock<Viewport> viewport;

struct TextInputVertex {
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

struct VSOutput {
    vec4 hPos : SV_Position;
    vec2 uv;
    vec3 viewPos;
    nointerpolation vec4 instanceColor;
    nointerpolation uint32_t glyphLoc;
}


struct PushData {
    int16_t4 *gpuAtlas;
};

static int16_t4 *gpuAtlas;
export int4 hb_gpu_fetch(int offset) {
    return int4(gpuAtlas[offset]);
}

[shader("vertex")]
VSOutput vertex(TextInputVertex input) {
    VSOutput o;

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
EncodedGBuffer pixel(VSOutput input, uniform int16_t4 *gpuAtlasAddress) {
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