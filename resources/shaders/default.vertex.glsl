#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 projection;
    mat4 view;
} ubo;

// Per vertex
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;

// Per instance
layout(location = 3) in vec4 instanceColor;
layout(location = 4) in mat4 instanceTransform;


layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 uv;

void main() {
    uv = inUV;
    gl_Position = ubo.projection * ubo.view * instanceTransform * vec4(inPosition, 1.0);
    fragColor = inColor * instanceColor.rgb;
}