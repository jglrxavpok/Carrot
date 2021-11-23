layout(push_constant) uniform PushConstant {
    layout(offset = 0) vec3 position;
    float scale;
    uvec4 uuid;
    vec3 color;
    uint textureID;
} billboard;