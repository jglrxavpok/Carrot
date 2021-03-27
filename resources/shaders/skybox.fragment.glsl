#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform samplerCube image;

layout(location = 0) in vec3 uv;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 fragmentColor = texture(image, uv);
    outColor = vec4(fragmentColor.rgb, 1.0);
}