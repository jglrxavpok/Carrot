/**
* Unpacks gbuffer data, without emissive, metallic+roughness nor motion vectors (to reduce strain on texture memory)
*/
GBuffer unpackGBufferLight(vec2 uv) {
    GBuffer gbuffer;

    gbuffer.albedo = texture(sampler2D(gAlbedo, gNearestSampler), uv);
    const vec3 viewPos = texture(sampler2D(gViewPos, gNearestSampler), uv).xyz;
    gbuffer.viewPosition = viewPos;

    gbuffer.intProperty = uint(texture(gIntPropertiesInput, uv).r);

    const vec4 viewNormalTangents = texture(sampler2D(gViewNormalTangents, gNearestSampler), uv);
    decompressTBN(viewNormalTangents,
                    (gbuffer.intProperty & IntPropertiesNegativeViewNormalZ) == IntPropertiesNegativeViewNormalZ,
                    (gbuffer.intProperty & IntPropertiesNegativeViewTangentZ) == IntPropertiesNegativeViewTangentZ,
                    (gbuffer.intProperty & IntPropertiesNegativeViewBitangent) == IntPropertiesNegativeViewBitangent,
                    gbuffer.viewTBN);

    gbuffer.entityID = texture(usampler2D(gEntityID, gNearestSampler), uv);

    return gbuffer;
}

GBufferSmall unpackGBufferLightTest(vec2 uv) {
    GBufferSmall gbuffer;

    gbuffer.albedo = texture(sampler2D(gAlbedo, gNearestSampler), uv);
    const vec3 viewPos = texture(sampler2D(gViewPos, gNearestSampler), uv).xyz;
    gbuffer.viewPosition = viewPos;

    uint intProperty = uint(texture(gIntPropertiesInput, uv).r);

    const vec4 viewNormalTangents = texture(sampler2D(gViewNormalTangents, gNearestSampler), uv);
    mat3 tbn;
    decompressTBN(viewNormalTangents,
                    (intProperty & IntPropertiesNegativeViewNormalZ) == IntPropertiesNegativeViewNormalZ,
                    (intProperty & IntPropertiesNegativeViewTangentZ) == IntPropertiesNegativeViewTangentZ,
                    (intProperty & IntPropertiesNegativeViewBitangent) == IntPropertiesNegativeViewBitangent,
                    tbn);
    gbuffer.viewT = tbn[0];
    gbuffer.viewN = tbn[2];

    return gbuffer;
}

GBuffer unpackGBuffer(vec2 uv) {
    GBuffer gbuffer = unpackGBufferLight(uv);

    vec4 metallicRoughnessVelocityXY = texture(sampler2D(gMetallicRoughnessValuesVelocityXY, gNearestSampler), uv);
    vec4 emissiveVelocityZ = texture(sampler2D(gEmissiveValuesVelocityZ, gNearestSampler), uv);
    gbuffer.metallicness = metallicRoughnessVelocityXY.x;
    gbuffer.roughness = metallicRoughnessVelocityXY.y;
    gbuffer.emissiveColor = emissiveVelocityZ.rgb;
    gbuffer.motionVector = vec3(metallicRoughnessVelocityXY.zw, emissiveVelocityZ.w);

    return gbuffer;
}

GBufferSmall unpackGBufferTest(vec2 uv) {
    GBufferSmall gbuffer = unpackGBufferLightTest(uv);

    vec4 metallicRoughnessVelocityXY = texture(sampler2D(gMetallicRoughnessValuesVelocityXY, gNearestSampler), uv);
    vec4 emissiveVelocityZ = texture(sampler2D(gEmissiveValuesVelocityZ, gNearestSampler), uv);
    gbuffer.metallicness = metallicRoughnessVelocityXY.x;
    gbuffer.roughness = metallicRoughnessVelocityXY.y;
    gbuffer.emissiveColor = emissiveVelocityZ.rgb;
    //gbuffer.motionVector = vec3(metallicRoughnessVelocityXY.zw, emissiveVelocityZ.w);

    return gbuffer;
}