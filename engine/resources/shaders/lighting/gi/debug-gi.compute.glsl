// Removes all cells from the grid, to start a new frame

#extension GL_EXT_debug_printf : enable
#extension GL_EXT_buffer_reference2 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable

#define HASH_GRID_SET_ID 0
#include "hash-grid.include.glsl"

const uint LOCAL_SIZE = 32;
layout (local_size_x = LOCAL_SIZE) in;
layout (local_size_y = LOCAL_SIZE) in;

layout(set = 1, binding = 0) uniform writeonly image2D outputImage;

#include <includes/gbuffer.glsl>
#include <includes/gbuffer_input.glsl>
DEFINE_GBUFFER_INPUTS(2)
#include <includes/gbuffer_unpack.glsl>

#include <includes/camera.glsl>
DEFINE_CAMERA_SET(3)


void main() {
    uvec2 pixel = gl_GlobalInvocationID.xy;

    uvec2 size = imageSize(outputImage);
    if(pixel.x >= size.x || pixel.y >= size.y) {
        return;
    }

    vec2 uv = vec2(pixel) / size;
    GBuffer g = unpackGBufferLight(uv);

    if(g.albedo.a < 0.00001f) {
        imageStore(outputImage, ivec2(pixel), vec4(pixel.x % 2, pixel.y % 2, 0, 1));
        return;
    }
    vec3 cameraPos = (cbo.inverseView * vec4(0,0,0,1)).xyz;
    vec3 worldPos = (cbo.inverseView * vec4(g.viewPosition, 1)).xyz;
    vec3 incomingRay = normalize(worldPos - cameraPos);

    HashCellKey key;
    key.cameraPos = cameraPos;
    key.hitPosition = worldPos;
    key.direction = incomingRay;
    uint cellIndex = hashGridFind(CURRENT_FRAME, key);
    vec4 giResult;
    if(cellIndex == InvalidCellIndex) {
        giResult = vec4(pixel.x/4 % 2, 0, pixel.y/4 % 2, 1);
    } else {
        giResult = vec4(hashGridRead(CURRENT_FRAME, cellIndex), 1) / hashGridGetSampleCount(CURRENT_FRAME, cellIndex);
    }
    imageStore(outputImage, ivec2(pixel), giResult);
}