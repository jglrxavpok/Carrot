#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int32 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_atomic_int64 : require
#extension GL_EXT_shader_image_int64 : require

#include "includes/buffers.glsl"
#include "includes/camera.glsl"
#include "includes/clusters.glsl"
#include "includes/materials.glsl"

MATERIAL_SYSTEM_SET(0)
layout(r64ui, set = 1, binding = 0) uniform u64image2D inputImage;
layout(set = 1, binding = 1, scalar) buffer ClusterRef {
    Cluster clusters[];
};

layout(set = 1, binding = 2, scalar) buffer ClusterInstanceRef {
    ClusterInstance instances[];
};

layout(set = 1, binding = 3, scalar) buffer ModelDataRef {
    InstanceData modelData[];
};
DEFINE_CAMERA_SET(2)

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

vec4 debugColor(uint triangleIndex, uint instanceIndex, uint clusterID);

void main() {
    uint64_t bufferValue = imageLoad(inputImage, uvToPixels(uv)).r;
    if(bufferValue == 0) {
        outColor = vec4(0,0,0,1);
        return;
    }
    uint triangleIndex = uint(bufferValue) & 0x7Fu; // low 7 bits
    uint instanceIndex = (uint(bufferValue) >> 7) & 0x1FFFFFFu; // high 25 bits: instance index
    uint clusterID = instances[instanceIndex].clusterID;

    outColor = debugColor(triangleIndex, instanceIndex, clusterID);
}