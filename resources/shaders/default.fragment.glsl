#version 450
#extension GL_ARB_separate_shader_objects : enable

struct MaterialData {
    uint textureIndex;
};

struct DrawData {
    uint materialIndex;
};

layout(constant_id = 0) const uint MAX_TEXTURES = 16;
layout(constant_id = 1) const uint MAX_MATERIALS = 16;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 uv;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform texture2D textures[MAX_TEXTURES];
layout(set = 0, binding = 2) uniform sampler linearSampler;
layout(set = 0, binding = 3) buffer MaterialBuffer {
    MaterialData materials[MAX_MATERIALS];
};

layout(push_constant) uniform DrawDataPushConstant {
    DrawData drawData[];
};

void main() {
    DrawData instanceDrawData = drawData[0]; // TODO: instancing
    MaterialData material = materials[instanceDrawData.materialIndex];
    outColor = vec4(texture(sampler2D(textures[material.textureIndex], linearSampler), uv).rgb * fragColor, 1.0);
}