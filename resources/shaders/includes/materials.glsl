#extension GL_EXT_scalar_block_layout : enable

struct Material {
    uint diffuseTexture;
    uint normalMap;
};

#define MATERIAL_SYSTEM_SET(setID) \
    layout(set = setID, binding = 0, scalar) buffer MaterialBuffer { Material materials[]; }; \
    layout(set = setID, binding = 1) uniform texture2D textures[]; \
    layout(set = setID, binding = 2) uniform sampler linearSampler; \
    layout(set = setID, binding = 3) uniform sampler nearestSampler;
