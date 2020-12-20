#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "draw_data.glsl"

layout(constant_id = 0) const uint MAX_TEXTURES = 16;
layout(constant_id = 1) const uint MAX_MATERIALS = 16;

struct MaterialData {
    uint textureIndex;
    bool ignoresInstanceColor;
};

layout(set = 0, binding = 1) uniform texture2D textures[MAX_TEXTURES];
layout(set = 0, binding = 2) uniform sampler linearSampler;

layout(set = 0, binding = 3) buffer MaterialBuffer {
    MaterialData materials[MAX_MATERIALS];
};

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec4 instanceColor;

layout(location = 0) out vec4 outColor;

void main() {
    DrawData instanceDrawData = drawData[0]; // TODO: instancing
    MaterialData material = materials[instanceDrawData.materialIndex];
    vec4 texColor = texture(sampler2D(textures[material.textureIndex], linearSampler), uv);
    if(!material.ignoresInstanceColor) {
        texColor *= instanceColor;
    }
    if(texColor.a < 0.01) {
        discard;
    }
    outColor = vec4(texColor.rgb * fragColor, 1.0);
}