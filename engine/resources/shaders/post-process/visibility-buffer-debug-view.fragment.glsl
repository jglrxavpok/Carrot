#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : enable
#extension GL_EXT_shader_image_int64 : enable

#include <includes/lighting_utils.glsl>

layout(r64ui, set = 0, binding = 0) uniform u64image2D inputImage;

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

ivec2 uvToPixels(vec2 uv) {
    ivec2 inputImageSize = imageSize(inputImage);
    return ivec2(uv * inputImageSize);
}

vec4 colors[] = {
    vec4(0,0,0,1),
    vec4(189.0f / 255.0f, 236.0f / 255.0f, 182.0f / 255.0f, 1.0f),
    vec4(108.0f / 255.0f, 112.0f / 255.0f,  89.0f / 255.0f, 1.0f),
    vec4(203.0f / 255.0f, 208.0f / 255.0f, 204.0f / 255.0f, 1.0f),
    vec4(250.0f / 255.0f, 210.0f / 255.0f,  01.0f / 255.0f, 1.0f),
    vec4(220.0f / 255.0f, 156.0f / 255.0f,   0.0f / 255.0f, 1.0f),
    vec4( 42.0f / 255.0f, 100.0f / 255.0f, 120.0f / 255.0f, 1.0f),
    vec4(120.0f / 255.0f, 133.0f / 255.0f, 139.0f / 255.0f, 1.0f),
    vec4(121.0f / 255.0f,  85.0f / 255.0f,  61.0f / 255.0f, 1.0f),
    vec4(157.0f / 255.0f, 145.0f / 255.0f,  01.0f / 255.0f, 1.0f),
    vec4(166.0f / 255.0f,  94.0f / 255.0f,  46.0f / 255.0f, 1.0f),
    vec4(203.0f / 255.0f,  40.0f / 255.0f,  33.0f / 255.0f, 1.0f),
    vec4(243.0f / 255.0f, 159.0f / 255.0f,  24.0f / 255.0f, 1.0f),
    vec4(250.0f / 255.0f, 210.0f / 255.0f,  01.0f / 255.0f, 1.0f),
    vec4(114.0f / 255.0f,  20.0f / 255.0f,  34.0f / 255.0f, 1.0f),
    vec4( 64.0f / 255.0f,  58.0f / 255.0f,  58.0f / 255.0f, 1.0f),
    vec4(157.0f / 255.0f, 161.0f / 255.0f, 170.0f / 255.0f, 1.0f),
    vec4(164.0f / 255.0f, 125.0f / 255.0f, 144.0f / 255.0f, 1.0f),
    vec4(248.0f / 255.0f,   0.0f / 255.0f,   0.0f / 255.0f, 1.0f),
    vec4(120.0f / 255.0f,  31.0f / 255.0f,  25.0f / 255.0f, 1.0f),
    vec4( 51.0f / 255.0f,  47.0f / 255.0f,  44.0f / 255.0f, 1.0f),
    vec4(180.0f / 255.0f,  76.0f / 255.0f,  67.0f / 255.0f, 1.0f),
    vec4(125.0f / 255.0f, 132.0f / 255.0f, 113.0f / 255.0f, 1.0f),
    vec4(161.0f / 255.0f,  35.0f / 255.0f,  18.0f / 255.0f, 1.0f),
    vec4(142.0f / 255.0f,  64.0f / 255.0f,  42.0f / 255.0f, 1.0f),
    vec4(130.0f / 255.0f, 137.0f / 255.0f, 143.0f / 255.0f, 1.0f),
};

vec4 triangleIndexToFloat(uint64_t index) {
    if(index == 0) {
        return colors[0];
    }
    return colors[uint8_t(index % 25ul + 1)];
}

void main() {
    uint64_t bufferValue = imageLoad(inputImage, uvToPixels(uv)).r;
    uint triangleIndex = uint(bufferValue) & 0xFFFFFFu; // low 24 bits
    uint instanceIndex = (uint(bufferValue) >> 24) & 0xFFu;

    outColor = triangleIndexToFloat(triangleIndex);
    //outColor = triangleIndexToFloat(instanceIndex);
}