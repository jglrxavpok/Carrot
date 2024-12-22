// Removes all cells from the grid, to start a new frame

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

#include <includes/gbuffer.glsl>
#include <includes/gbuffer_input.glsl>
DEFINE_GBUFFER_INPUTS(2)
#include <includes/gbuffer_unpack.glsl>

#include <includes/camera.glsl>
DEFINE_CAMERA_SET(3)

layout(push_constant) uniform PushConstant {
    uint frameCount;
};

void main() {
    uvec2 pixel = gl_GlobalInvocationID.xy;

    uvec2 size = imageSize(outputImage);
    if(pixel.x >= size.x || pixel.y >= size.y) {
        return;
    }

    vec2 uv = vec2(pixel) / size;
    GBuffer g = unpackGBufferLight(uv);

    if(g.albedo.a < 0.00001f) {
        imageStore(outputImage, ivec2(pixel), vec4(0, 0, 0, 0));
        return;
    }
    vec3 cameraPos = (cbo.inverseView * vec4(0,0,0,1)).xyz;
    vec3 worldPos = (cbo.inverseView * vec4(g.viewPosition, 1)).xyz;
    vec3 incomingRay = normalize(worldPos - cameraPos);

    RandomSampler rng;
    initRNG(rng, uv, size.x, size.y, frameCount);

    GIInputs giInputs;
    giInputs.hitPosition = worldPos;
    giInputs.cameraPosition = cameraPos;
    giInputs.incomingRay = incomingRay;
    giInputs.frameIndex = frameCount;
    giInputs.surfaceNormal = g.viewTBN[2];
    giInputs.metallic = g.metallicness;
    giInputs.roughness = g.roughness;
    giInputs.surfaceColor = g.albedo.rgb;

    vec3 giResult = giGetCurrentFrame(rng, giInputs);
    imageStore(outputImage, ivec2(pixel), vec4(giResult, 1));
}