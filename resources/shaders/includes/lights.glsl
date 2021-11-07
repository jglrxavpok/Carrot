struct Light {
    vec3 position;
    float intensity;
    vec3 direction;
    uint type;
    vec3 color;
    bool enabled;
};

#define LIGHT_BUFFER(setID, bindingID) \
layout(set = setID, binding = bindingID) buffer Lights { \
    vec3 ambientColor; \
    uint count; \
    Light l[]; \
} lights;

#define LIGHT_SET(setID) LIGHT_BUFFER(setID, 0)