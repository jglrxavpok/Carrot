import modules.base;
import modules.camera;
import modules.gbuffer;
import modules.per_object;
import modules.materials;

ParameterBlock<Camera> camera;
ParameterBlock<MaterialSystem> materialSystem;
ParameterBlock<PerObjectDrawBuffers> objects;

mat3 computeNormalMappingMatrix(in VSGBufferOutput input) {
    vec3 N_ = normalize(input.N);
    vec3 T_ = normalize(input.T - dot(input.T, N_) * N_);

    vec3 B_ = normalize(input.bitangentSign * cross(N_, T_));

    return mat3(input.T, B_, input.N);
}

[shader("pixel")]
EncodedGBuffer pixel(in VSGBufferOutput input) {
    GBuffer result;

    const Material material = materialSystem.materials[objects.Load(input.inDrawID).materialIndex];
    const MaterialOutput output = material.Compute(materialSystem, materialSystem.linearSampler, input.uv);

    if (output.albedo.a < 0.01) {
        discard;
    }

    result.albedo = output.albedo + vec4(result.emissiveColor, 0);
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
    result.motionVector = previousClipPos.xyz / previousClipPos.w - clipPos.xyz / clipPos.w;

    return EncodedGBuffer(result);
}