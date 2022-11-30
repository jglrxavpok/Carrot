#define IntPropertiesRayTracedLighting (1u << 0u)
#define IntPropertiesNegativeNormalZ (1u << 1u)
#define IntPropertiesNegativeTangentZ (1u << 2u)

struct GBuffer {
    vec4 albedo;
    vec3 viewPosition;
    vec3 viewNormal;
    vec3 viewTangent;

    uint intProperty;
    uvec4 entityID;

    float metallicness;
    float roughness;

    vec3 emissiveColor;
    vec3 motionVector;
};
