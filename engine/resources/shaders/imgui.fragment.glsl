#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec4 inColor;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform texture2D texture;

void main() {
    outColor = vec4(inColor);
}