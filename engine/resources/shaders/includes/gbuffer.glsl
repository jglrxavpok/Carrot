#define IntPropertiesRayTracedLighting          (1u << 0u)
#define IntPropertiesNegativeViewNormalZ        (1u << 1u)
#define IntPropertiesNegativeViewTangentZ       (1u << 2u)
#define IntPropertiesNegativeViewBitangent      (1u << 3u)

struct GBuffer {
    vec4 albedo;
    vec3 viewPosition;
    mat3 viewTBN;

    uint intProperty;
    uvec4 entityID;

    float metallicness;
    float roughness;

    vec3 emissiveColor;
    vec3 motionVector;
};

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