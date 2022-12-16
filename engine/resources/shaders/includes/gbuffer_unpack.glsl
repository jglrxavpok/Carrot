GBuffer unpackGBuffer(vec2 uv) {
    GBuffer gbuffer;

    gbuffer.albedo = texture(sampler2D(gAlbedo, gLinearSampler), uv);
    vec3 viewPos = texture(sampler2D(gViewPos, gLinearSampler), uv).xyz;
    gbuffer.viewPosition = viewPos;
    gbuffer.intProperty = uint(texture(gIntPropertiesInput, uv).r);

    vec4 compressedNormalTangent = texture(sampler2D(gViewNormalTangents, gLinearSampler), uv);
    vec3 viewNormal = compressedNormalTangent.xyx;
    viewNormal.z = sqrt(max(0, 1 - dot(viewNormal.xy, viewNormal.xy)));
    if((gbuffer.intProperty & IntPropertiesNegativeNormalZ) == IntPropertiesNegativeNormalZ) {
        viewNormal.z *= -1;
    }

    vec3 viewTangent = compressedNormalTangent.zwx;
    viewTangent.z = sqrt(max(0, 1 - dot(viewTangent.xy, viewTangent.xy)));
    if((gbuffer.intProperty & IntPropertiesNegativeTangentZ) == IntPropertiesNegativeTangentZ) {
        viewTangent.z *= -1;
    }

    gbuffer.viewNormal = viewNormal;
    gbuffer.viewTangent = viewTangent;

    gbuffer.entityID = uvec4(0,0,0,0); // TODO

    vec2 metallicRoughness = texture(sampler2D(gMetallicRoughnessValues, gLinearSampler), uv).rg;
    gbuffer.metallicness = metallicRoughness.x;
    gbuffer.roughness = metallicRoughness.y;
    gbuffer.emissiveColor = texture(sampler2D(gEmissiveValues, gLinearSampler), uv).rgb;
    gbuffer.motionVector = texture(sampler2D(gMotionVectors, gLinearSampler), uv).xyz;

    return gbuffer;
}