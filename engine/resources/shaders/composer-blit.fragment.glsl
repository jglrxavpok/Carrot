layout(constant_id = 0) const uint MAX_TEXTURES = 16;
layout(set = 0, binding = 0) uniform sampler linearSampler;
layout(set = 0, binding = 1) uniform texture2D textures[MAX_TEXTURES];

layout(location = 0) in vec2 uv;
layout(location = 1) flat in uint texIndex;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 fragmentColor = texture(sampler2D(textures[texIndex], linearSampler), uv);
    outColor = fragmentColor;
}