import modules.base;
import modules.camera;
import modules.materials;
import modules.gbuffer;
import modules.viewport;
import modules.per_object;
import modules.harfbuzz;

[vk::binding(0, 0)]
ParameterBlock<Viewport> viewport;

[vk::binding(0, 1)]
ParameterBlock<Camera> camera;

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

    uint8_t is2D;
};

static int16_t4 *gpuAtlas;
export int4 hb_gpu_fetch(int offset) {
    return int4(gpuAtlas[offset]);
}

[shader("vertex")]
VSOutput vertex(TextInputVertex input, uniform PushData pushData) {
    VSOutput o;

    const CameraBufferObject cbo = camera.CurrentFrame();
    mat4 modelview = cbo.view * input.instanceTransform;
    mat4 proj = cbo.jitteredProjection;
    mat4 modelviewproj = proj * modelview;

    float3 inputPosition = float3(input.position.x, 0, input.position.y);
    if (pushData.is2D) {
        proj = IdentityMatrix;
        modelview = input.instanceTransform;
        modelviewproj = input.instanceTransform;

        inputPosition = float3((input.position / viewport.frameDimensions)*2, 0);
    } else {
        inputPosition = inputPosition.xzy;
    }

    float4 jac = vec4(input.emPerPos, 0.0, 0.0, -input.emPerPos);
    float4 viewPos = modelview * float4(inputPosition, 1);
    // harfbuzzDilateVertex(input.position, input.texcoord, input.normal, jac, modelviewproj, viewport.frameDimensions);
    o.hPos = proj * viewPos;
    o.uv = input.texcoord;
    o.viewPos = viewPos.xyz;
    o.glyphLoc = input.glyphLoc;
    o.instanceColor = input.instanceColor;

    return o;
}

[shader("pixel")]
EncodedGBuffer gBufferPixel(VSOutput input, uniform PushData pushData) {
    gpuAtlas = pushData.gpuAtlas;
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

[shader("pixel")]
float4 uiPixel(VSOutput input, uniform PushData pushData) {
    gpuAtlas = pushData.gpuAtlas;

    float coverage = 0;
    float4 albedo = harfbuzzPaint(input.uv, input.glyphLoc, input.instanceColor, coverage);
    return float4(albedo.rgb, albedo.a * coverage);
}