import modules.base;
import modules.camera;
import modules.materials;
import modules.gbuffer;
import modules.per_object;

[vk::binding(0, 0)]
ParameterBlock<MaterialSystem> materialSystem;

[vk::binding(0, 1)]
ParameterBlock<Camera> camera;

[vk::binding(0, 2)]
ParameterBlock<PerObjectDrawBuffers> objects;

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
    if (color < 0.01) {
        discard;
    }

    GBuffer gbuffer;

    gbuffer.albedo = vec4(1, 1, 1, 1) * input.instanceColor;
    gbuffer.viewPosition = input.viewPos;
    gbuffer.intProperty = IntProperties::RayTracedLighting;

    return EncodedGBuffer(gbuffer);
}