#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int32 : require
#extension GL_EXT_shader_atomic_int64 : require
#extension GL_EXT_shader_image_int64 : require

#include "draw_data.glsl"
#include "includes/materials.glsl"

layout(location = 0) in vec4 ndcPosition;
layout(location = 1) in flat uint instanceID;

layout(r64ui, set = 0, binding = 2) uniform u64image2D outputImage;

void main() {
    ivec2 imageSize = imageSize(outputImage);

    vec4 ndc = ndcPosition;
    if(ndc.z < 0) {
        discard;
    }

    ndc.xyz /= ndc.w;
    vec2 pixelCoordsFloat = (ndc.xy + 1.0) / 2.0 * imageSize;

    ivec2 pixelCoords = ivec2(pixelCoordsFloat);
    uint depth = 0xFFFFFFFFu - uint(double(ndc.z) * 0xFFFFFFFFu);
    // 32 high bits: depth
    // 32 low bits: 25 high bits instance ID, 7 remaining: triangle index
    uint low = ((instanceID & 0x1FFFFFFu) << 7) | (uint(gl_PrimitiveID) & 0x7Fu);

    uint64_t value = pack64(u32vec2(low, depth));
    imageAtomicMax(outputImage, pixelCoords, value);
}