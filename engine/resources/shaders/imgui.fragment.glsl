#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec4 inColor;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D textures[1024];

layout(push_constant) uniform DisplayRect {
    vec2 Translation;
    vec2 Size;
    uint TextureIndex;
} Display;

void main() {
    vec4 textureColor = texture(textures[Display.TextureIndex], inUV);
    outColor = inColor * textureColor;
}