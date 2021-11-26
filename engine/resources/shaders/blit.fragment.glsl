layout(set = 0, binding = 0) uniform texture2D image;
layout(set = 0, binding = 1) uniform sampler linearSampler;

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 fragmentColor = texture(sampler2D(image, linearSampler), uv);
    outColor = fragmentColor;
}