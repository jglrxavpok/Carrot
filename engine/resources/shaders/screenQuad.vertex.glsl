// Per vertex
layout(location = 0) in vec2 inPosition;

layout(location = 0) out vec2 uv;

void main() {
    uv = (inPosition+vec2(1.0))/2;
    gl_Position = vec4(inPosition, 0, 1);
}