// Per vertex
layout(location = 0) in vec2 inPosition;

layout(location = 0) out vec2 uv;
layout(location = 1) flat out uint texIndex;

layout(push_constant) uniform Region {
    layout(offset = 24) float left;
    float right;
    float top;
    float bottom;
    float depth;
    uint texIndex;
} region;

void main() {
    uv = (inPosition+vec2(1.0))/2;
    vec2 screenPosition = uv * vec2(region.right - region.left, region.top - region.bottom) + vec2(region.left, region.bottom);
    gl_Position = vec4(screenPosition, region.depth, 1);
    texIndex = region.texIndex;
}