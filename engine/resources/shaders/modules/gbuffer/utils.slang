implementing gbuffer;

void decompressTBN(vec4 compressed, bool negativeNormal, bool negativeTangent, bool negativeBitangent, out mat3 outTBN) {
    vec3 normal = compressed.xyx;
    normal.z = sqrt(max(0, 1 - dot(normal.xy, normal.xy)));
    normal.z *= negativeNormal ? -1 : 1;

    vec3 tangent = compressed.zwx;
    tangent.z = sqrt(max(0, 1 - dot(tangent.xy, tangent.xy)));
    tangent.z *= negativeTangent ? -1 : 1;

    vec3 bitangent = cross(tangent, normal) * (negativeBitangent ? -1 : 1);
    outTBN = mat3(tangent, bitangent, normal);
}

void compressTBN(mat3 tbn, out vec4 compressed, out bool negativeNormal, out bool negativeTangent, out bool negativeBitangent) {
    vec3 tangent = normalize(tbn[0]);
    vec3 normal = normalize(tbn[2]);
    compressed = vec4(normal.xy, tangent.xy);
    negativeNormal = normal.z < 0;
    negativeTangent = tangent.z < 0;
    negativeBitangent = dot(tbn[1], cross(tangent, normal)) < 0;
}