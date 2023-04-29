#extension GL_EXT_scalar_block_layout : enable

struct Material {
    vec4 baseColor;

    vec3 emissiveColor;
    uint emissive;

    vec2 metallicRoughnessFactor;

    uint albedo;
    uint normalMap;
    uint metallicRoughness;
};

#define MATERIAL_SYSTEM_SET(setID)                                                                                      \
    layout(set = setID, binding = 0, scalar) buffer MaterialBuffer { Material materials[]; };                           \
    layout(set = setID, binding = 1) uniform texture2D textures[];                                                      \
    layout(set = setID, binding = 2) uniform sampler linearSampler;                                                     \
    layout(set = setID, binding = 3) uniform sampler nearestSampler;                                                    \
    layout(set = setID, binding = 4) uniform GlobalTextures {                                                           \
        uint dithering;                                                                                                 \
        uint blueNoise;                                                                                                 \
    } globalTextures;                                                                                                   \
                                                                                                                        \
    float dither(uvec2 coords) {                                                                                        \
        const uint DITHER_SIZE = 8;                                                                                     \
        const vec2 ditherUV = vec2(coords % DITHER_SIZE) / DITHER_SIZE;                                                 \
        return texture(sampler2D(textures[globalTextures.dithering], nearestSampler), ditherUV).r;                      \
    }                                                                                                                   \


