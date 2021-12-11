struct Light {
    vec3 position;
    float intensity;
    vec3 direction;
    uint type;
    vec3 color;
    bool enabled;

    // point light
    float constantAttenuation;
    float linearAttenuation;
    float quadraticAttenuation;

    // spot light
    float cutoffCosAngle;
    float outerCosCutoffAngle;
};

#define LIGHT_BUFFER(setID, bindingID) \
layout(set = setID, binding = bindingID) buffer Lights { \
    vec3 ambientColor; \
    uint count; \
    Light l[]; \
} lights;

#define LIGHT_SET(setID) LIGHT_BUFFER(setID, 0)

#define POINT_LIGHT_TYPE 0
#define DIRECTIONAL_LIGHT_TYPE 1
#define SPOT_LIGHT_TYPE 2