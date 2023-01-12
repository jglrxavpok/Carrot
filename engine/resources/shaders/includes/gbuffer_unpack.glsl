/**
* Unpacks gbuffer data, without emissive, metallic+roughness nor motion vectors (to reduce strain on texture memory)
*/
GBuffer unpackGBufferLight(vec2 uv) {
    GBuffer gbuffer;

    gbuffer.albedo = texture(sampler2D(gAlbedo, gLinearSampler), uv);
    vec3 viewPos = texture(sampler2D(gViewPos, gLinearSampler), uv).xyz;
    gbuffer.viewPosition = viewPos;
    gbuffer.intProperty = uint(texture(gIntPropertiesInput, uv).r);

    decompressTBN(texture(sampler2D(gViewNormalTangents, gLinearSampler), uv),
                    (gbuffer.intProperty & IntPropertiesNegativeViewNormalZ) == IntPropertiesNegativeViewNormalZ,
                    (gbuffer.intProperty & IntPropertiesNegativeViewTangentZ) == IntPropertiesNegativeViewTangentZ,
                    (gbuffer.intProperty & IntPropertiesNegativeViewBitangent) == IntPropertiesNegativeViewBitangent,
                    gbuffer.viewTBN);

    gbuffer.entityID = texture(usampler2D(gEntityID, gNearestSampler), uv);

    return gbuffer;
}

GBuffer unpackGBuffer(vec2 uv) {
    GBuffer gbuffer = unpackGBufferLight(uv);

    vec4 metallicRoughnessVelocityXY = texture(sampler2D(gMetallicRoughnessValuesVelocityXY, gLinearSampler), uv);
    vec4 emissiveVelocityZ = texture(sampler2D(gEmissiveValuesVelocityZ, gLinearSampler), uv);
    gbuffer.metallicness = metallicRoughnessVelocityXY.x;
    gbuffer.roughness = metallicRoughnessVelocityXY.y;
    gbuffer.emissiveColor = emissiveVelocityZ.rgb;
    gbuffer.motionVector = vec3(metallicRoughnessVelocityXY.zw, emissiveVelocityZ.w);

    return gbuffer;
}