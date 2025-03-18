// Removes all cells from the grid, to start a new frame
#include <lighting/gi/types.glsl>
#include <includes/number-print.glsl>

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

layout(set = 1, binding = 1, scalar) buffer ScreenProbes {
    ScreenProbe[] probes;
};

layout(set = 1, binding = 2, scalar) buffer SpawnedProbes {
    uint spawnedProbeCount;
    uint[] spawnedProbes;
};

layout(set = 1, binding = 3, scalar) buffer EmptyProbes {
    uint count;
    uint[] indices; // index into 'spawnedProbes'
} emptyProbes;
layout(set = 1, binding = 4, scalar) buffer ReprojectedProbes {
    uint count;
    uint[] indices; // index into 'spawnedProbes'
} reprojectedProbes;

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

    /*
    #if 0
    bool isEmpty = false;
    for(int i = 0; i < emptyProbes.count; i++) {
        if(emptyProbes.indices[i] == probeIndex) {
            isEmpty = true;
            break;
        }
    }
    #else
    bool isEmpty = true;
    for(int i = 0; i < reprojectedProbes.count; i++) {
        if(spawnedProbes[reprojectedProbes.indices[i]] == probeIndex) {
            isEmpty = false;
            break;
        }
    }
    #endif*/

    ScreenProbe probe = probes[probeIndex];
    imageStore(outputImage, ivec2(pixel), vec4(unpackRadiance(probe.radiance) / probe.sampleCount, 1));
    //imageStore(outputImage, ivec2(pixel), vec4(vec2(probe.bestPixel)/vec2(size),0,1));
    /*if(isEmpty) {
        imageStore(outputImage, ivec2(pixel), vec4(1, 0, 0, 1));
    } else {
        imageStore(outputImage, ivec2(pixel), vec4(0,0,0,1));
    }*/

    /*
    vec2 uv = vec2(pixel) / vec2(size);
    vec2 startPosition = vec2(0.5);
    vec2 fontSize = vec2(0.05f);
    float digit = PrintValue(uv, startPosition, fontSize, emptyProbes.count, 2.0f, 0);
    const vec4 digitColor = vec4(1, 0, 0, 1);
    const vec4 objectColor = vec4(0,0,0,1);
    vec4 color = digit * digitColor + (1 - digit) * objectColor;
    imageStore(outputImage, ivec2(pixel), color);*/
}