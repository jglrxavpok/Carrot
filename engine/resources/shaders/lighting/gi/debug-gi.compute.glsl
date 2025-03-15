// Removes all cells from the grid, to start a new frame

#extension GL_EXT_debug_printf : enable
#extension GL_EXT_buffer_reference2 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable

#define HASH_GRID_SET_ID 0
#include "hash-grid.include.glsl"

layout(push_constant) uniform PushConstant {
    uint frameCount;
    uint frameWidth;
    uint frameHeight;
} push;

const uint LOCAL_SIZE = 32;
layout (local_size_x = LOCAL_SIZE) in;
layout (local_size_y = LOCAL_SIZE) in;

layout(set = 1, binding = 0) uniform writeonly image2D outputImage;

struct ScreenProbe {
    ivec3 radiance;
    uint sampleCount;
    vec3 worldPos;
    vec3 normal;
};

layout(set = 1, binding = 1, scalar) buffer ScreenProbes {
    ScreenProbe[] probes;
};

#include <includes/gbuffer.glsl>
#include <includes/gbuffer_input.glsl>
DEFINE_GBUFFER_INPUTS(2)
#include <includes/gbuffer_unpack.glsl>

#include <includes/camera.glsl>
DEFINE_CAMERA_SET(3)

const uint ScreenProbeSize = 8;

void main() {
    uvec2 pixel = gl_GlobalInvocationID.xy;

    uvec2 size = imageSize(outputImage);
    if(pixel.x >= size.x || pixel.y >= size.y) {
        return;
    }

    uvec2 probePosition = pixel / ScreenProbeSize + uvec2(push.frameCount*0);
    const uint probesPerWidth = (push.frameWidth+ScreenProbeSize-1) / ScreenProbeSize;
    uint probeIndex = probePosition.x + probePosition.y * probesPerWidth;
    ScreenProbe probe = probes[probeIndex];
    imageStore(outputImage, ivec2(pixel), vec4(unpackRadiance(probe.radiance) / probe.sampleCount, 1));
}