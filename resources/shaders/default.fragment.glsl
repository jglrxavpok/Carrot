#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 uv;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform texture2D albedo[2];
layout(set = 0, binding = 2) uniform sampler linearSampler;

void main() {
    int index = 0;
    if(uv.x > 0.5)
        index = 1;
    outColor = vec4(texture(sampler2D(albedo[index], linearSampler), uv).rgb * fragColor, 1.0);
}