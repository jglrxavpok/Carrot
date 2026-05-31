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

struct Input {
    BaseVertex<false> vertex; // to remove padding/not padding support, switch to vertex pulling
    InstanceData instance;
}

[shader("vertex")]
VSGBufferOutput vertex(in Input input, int drawID: SV_DrawIndex) {
    VSGBufferOutput out;

    BaseVertex<false> vertex = input.vertex;
    InstanceData instance = input.instance;

    const float4x4 modelview = camera.CurrentFrame().view * instance.transform;
    const float4x4 previousFrameModelview = camera.PreviousFrame().view * instance.lastFrameTransform;
    const float4 viewPosition = modelview * vertex.getPos();
    const float4 previousFrameViewPosition = previousFrameModelview * vertex.getPos();
    out.svPos = camera.CurrentFrame().jitteredProjection * viewPosition;

    out.vertexColor = float4(vertex.getColor(), 1) * instance.color;
    out.viewPosition = viewPosition.xyz / viewPosition.w;
    out.uv = vertex.getUV();

    // TODO: rename
    out.previousFrameViewPosition = (camera.PreviousFrame().jitteredProjection * float4(previousFrameViewPosition.xyz / previousFrameViewPosition.w, 1)).xyw;

    float3 t = normalize(vertex.getTangent().xyz);
    float3 n = normalize(vertex.getNormal());
    float3x3 normalMatrix = transpose(inverse(float3x3(modelview)));
    out.N = normalize(normalMatrix * n);
    out.T = normalize(normalMatrix * (t - dot(t, n) * n));
    out.bitangentSign = vertex.getTangent().w;
    out.uuid = instance.uuid;
    out.inModelview = modelview;

    out.drawID = drawID;

    return out;
}

mat3 computeNormalMappingMatrix(in VSGBufferOutput input) {
    vec3 N_ = normalize(input.N);
    vec3 T_ = normalize(input.T - dot(input.T, N_) * N_);

    vec3 B_ = normalize(input.bitangentSign * cross(N_, T_));

    return mat3(input.T, B_, input.N);
}

GBuffer defaultGBuffer(VSGBufferOutput input, float4 fragCoords) {
    GBuffer result;

    const ObjectDrawData object = objects.Load(NonUniformResourceIndex(input.drawID));
    const Material material = materialSystem.materials[NonUniformResourceIndex(object.materialIndex)];
    const MaterialOutput output = material.Compute(materialSystem, materialSystem.linearSampler, input.uv);

    vec4 albedo = output.albedo;
    albedo *= input.vertexColor;
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

    const mat3 toView = computeNormalMappingMatrix(input);
    result.viewTBN = toView * mat3(vec3(1, 0, 0), vec3(0, 1, 0), output.normal);
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
    return result;
}

[shader("pixel")]
EncodedGBuffer pixel(in VSGBufferOutput input, float4 fragCoords: SV_Position) {
    return EncodedGBuffer(defaultGBuffer(input, fragCoords));
}

[shader("pixel")]
EncodedGBuffer blockout(in VSGBufferOutput input, float4 fragCoords: SV_Position) {
    float3 worldPos = (camera.CurrentFrame().inverseView * float4(input.viewPosition, 1)).xyz;

    int3 roundedPos = int3(round(worldPos));

    // color of splash screen (FF8146)
    float4 color1 = float4(0xFF / 255.0f, 0x81 / 255.0f, 0x46 / 255.0f, 1);
    float4 color2 = float4(0x30 / 255.0f, 0x30 / 255.0f, 0x30 / 255.0f, 1);
    float4 texColor = ((roundedPos.x + roundedPos.y + roundedPos.z) & 1) == 0 ? color1 : color2;

    GBuffer result = defaultGBuffer(input, fragCoords);
    result.albedo *= float4(texColor.rgb, 1);
    return EncodedGBuffer(result);
}

[shader("pixel")]
float4 unlit(in VSGBufferOutput input, float4 fragCoords: SV_Position) {
    const ObjectDrawData object = objects.Load(NonUniformResourceIndex(input.drawID));
    const Material material = materialSystem.materials[NonUniformResourceIndex(object.materialIndex)];
    const MaterialOutput output = material.Compute(materialSystem, materialSystem.linearSampler, input.uv);
    vec4 albedo = output.albedo;
    albedo *= input.vertexColor;
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
    return albedo;
}