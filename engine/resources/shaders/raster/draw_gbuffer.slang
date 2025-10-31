import modules.base;
import modules.camera;
import modules.gbuffer;
import modules.per_object;
import modules.materials;
import modules.viewport;

ParameterBlock<Camera> camera;
ParameterBlock<MaterialSystem> materialSystem;
ParameterBlock<PerObjectDrawBuffers> objects;
ParameterBlock<Viewport> viewport;

mat3 computeNormalMappingMatrix(in VSGBufferOutput input) {
    vec3 N_ = normalize(input.N);
    vec3 T_ = normalize(input.T - dot(input.T, N_) * N_);

    vec3 B_ = normalize(input.bitangentSign * cross(N_, T_));

    return mat3(input.T, B_, input.N);
}

[shader("pixel")]
EncodedGBuffer pixel(in VSGBufferOutput input, float4 fragCoords: SV_Position) {
    GBuffer result;

    const ObjectDrawData object = objects.Load(NonUniformResourceIndex(input.inDrawID));
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

    vec4 clipPos = camera.CurrentFrame().nonJitteredProjection * vec4(input.viewPosition, 1.0);
    vec4 previousClipPos = camera.PreviousFrame().nonJitteredProjection * vec4(input.previousFrameViewPosition, 1.0);
    previousClipPos.xyz /= previousClipPos.w;
    clipPos.xyz /= clipPos.w;

    previousClipPos.xy = (previousClipPos.xy+1) / 2.0f;
    clipPos.xy = (clipPos.xy+1) / 2.0f;
    

    result.motionVector = float3(previousClipPos.xy - clipPos.xy, 0);

    return EncodedGBuffer(result);
}
