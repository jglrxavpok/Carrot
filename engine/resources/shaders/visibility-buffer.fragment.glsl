#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int32 : require
#extension GL_EXT_shader_atomic_int64 : require
#extension GL_EXT_shader_image_int64 : require

#include "draw_data.glsl"
#include "includes/materials.glsl"

layout(r64ui, set = 0, binding = 1) uniform u64image2D outputImage;


layout(location = 0) in vec4 ndcPosition;
layout(location = 1) in flat int drawID;
layout(location = 2) in flat int debugInt;

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
    // 32 low bits: 8 high bits instance ID, 24 remaining: triangle index (will change with cluster rendering)
    uint instanceIndex = 0; // TODO
    uint low = ((instanceIndex & 0xFFu) << 24) | (uint(gl_PrimitiveID+1) & 0xFFFFFFu);
    //uint low = ((instanceIndex & 0xFFu) << 24) | (uint(debugInt+1) & 0xFFFFFFu);

    uint64_t value = pack64(u32vec2(low, depth));
    imageAtomicMax(outputImage, pixelCoords, value);
}