struct Light {
    vec3 color;
    uint8_t flags;
    uint8_t type;
    float intensity;

    vec3 v0; // point: position, spot: position, directional: direction
    vec3 v1; // point: attenuation factors, spot: direction, directional: nothing
    vec2 v2; // point: nothing, spot: cutoff angles, directional: nothing
};

#define LIGHT_BUFFER(setID, bindingID) \
layout(set = setID, binding = bindingID, scalar) buffer Lights { \
    vec3 ambientColor; \
    uint count; \
    vec3 fogColor; \
    float fogDistance; \
    float fogDepth; \
    Light l[]; \
} lights;

#define LIGHT_SET(setID)                                                                        \
    LIGHT_BUFFER(setID, 0)                                                                      \
    layout(set = setID, binding = 1) buffer ActiveLights {                                      \
        uint count;                                                                             \
        uint indices[];                                                                         \
    } activeLights;

#define POINT_LIGHT_TYPE 0
#define DIRECTIONAL_LIGHT_TYPE 1
#define SPOT_LIGHT_TYPE 2