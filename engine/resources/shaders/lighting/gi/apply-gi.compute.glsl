// Removes all cells from the grid, to start a new frame
#include <lighting/gi/types.glsl>

#extension GL_EXT_debug_printf : enable
#extension GL_EXT_buffer_reference2 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable

#include <includes/materials.glsl>
MATERIAL_SYSTEM_SET(4)
#include <includes/rng.glsl>

#define HASH_GRID_SET_ID 0
#include "gi-interface.include.glsl"

const uint LOCAL_SIZE = 32;
layout (local_size_x = LOCAL_SIZE) in;
layout (local_size_y = LOCAL_SIZE) in;

layout(set = 1, binding = 0) uniform writeonly image2D outputImage;

layout(set = 1, binding = 1, scalar) buffer ScreenProbes {
    ScreenProbe[] probes;
};

#include <includes/gbuffer.glsl>
#include <includes/gbuffer_input.glsl>
DEFINE_GBUFFER_INPUTS(2)
#include <includes/gbuffer_unpack.glsl>

#include <includes/camera.glsl>
DEFINE_CAMERA_SET(3)

layout(push_constant) uniform PushConstant {
    uint frameCount;
    uint frameWidth;
    uint frameHeight;
} push;

const int FILTER_RADIUS = 2;
const float KERNEL_WEIGHTS[FILTER_RADIUS*2+1] = {
    1.0f/16.0f,
    1.0f/4.0f,
    3.0f/8.0f,
    1.0f/4.0f,
    1.0f/16.0f,
};
const uint ScreenProbeSize = 8;

uint getProbeIndex(uvec2 probePosition) {
    const uint probesPerWidth = (push.frameWidth+ScreenProbeSize-1) / ScreenProbeSize;
    return probePosition.x + probePosition.y * probesPerWidth;
}

vec3 readProbe(uvec2 probePos) {
    const ScreenProbe probe = probes[getProbeIndex(probePos)];
    return unpackRadiance(probe.radiance) / probe.sampleCount;
}

void main() {
    uvec2 pixel = gl_GlobalInvocationID.xy;

    uvec2 size = imageSize(outputImage);
    if(pixel.x >= size.x || pixel.y >= size.y) {
        return;
    }

    vec3 average = vec3(0,0,0);
    const uint probesPerWidth = (push.frameWidth+ScreenProbeSize-1) / ScreenProbeSize;

    // bilinear interpolation
    pixel -= uvec2(ScreenProbeSize/2);
    const uvec2 probePositionTopLeft = uvec2(pixel) / ScreenProbeSize + uvec2(push.frameCount*0);
    const uvec2 probePositionTopRight = probePositionTopLeft + uvec2(1, 0);
    const uvec2 probePositionBottomLeft = probePositionTopLeft + uvec2(0, 1);
    const uvec2 probePositionBottomRight = probePositionTopLeft + uvec2(1, 1);
    const float w11 = (probePositionTopRight.x*ScreenProbeSize - pixel.x)*(probePositionTopRight.y*ScreenProbeSize - pixel.y);
    const float w12 = (probePositionTopRight.x*ScreenProbeSize - pixel.x)*(pixel.y - probePositionBottomRight.y*ScreenProbeSize);
    const float w21 = (pixel.x - probePositionTopLeft.x*ScreenProbeSize)*(probePositionTopRight.y*ScreenProbeSize - pixel.y);
    const float w22 = (pixel.x - probePositionTopLeft.x*ScreenProbeSize)*(pixel.y - probePositionBottomRight.y*ScreenProbeSize);
    average += readProbe(probePositionBottomLeft) * w11;
    average += readProbe(probePositionBottomRight) * w21;
    average += readProbe(probePositionTopLeft) * w12;
    average += readProbe(probePositionTopRight) * w22;

    const float denominator = w11 + w12 + w21 + w22;
    const float invDenominator = 1.0f / denominator;
    average *= invDenominator;

    imageStore(outputImage, ivec2(pixel), vec4(average, 1));
}