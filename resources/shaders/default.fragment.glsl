#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 uv;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform texture2D albedo;
layout(set = 0, binding = 2) uniform sampler linearSampler;

void main() {
    outColor = vec4(texture(sampler2D(albedo, linearSampler), uv).rgb * fragColor, 1.0);
}