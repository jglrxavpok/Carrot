#extension GL_ARB_separate_shader_objects : enable

// mostly based on ImGui's Vulkan backend

// Per vertex
layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 2) in uint inColor;

layout(location = 0) out vec2 outPosition;
layout(location = 1) out vec2 outUV;
layout(location = 2) out vec4 outColor;

layout(push_constant) uniform DisplayRect {
    vec2 Translation;
    vec2 Size;
    uint TextureIndex;
} Display;

void main() {
    // TODO: camera
    outPosition = inPosition;
    outUV = inUV;
    float alpha = ((inColor >> 24) & 0xFFu) / 255.0f;
    float blue = ((inColor >> 16) & 0xFFu) / 255.0f;
    float green = ((inColor >> 8) & 0xFFu) / 255.0f;
    float red = ((inColor >> 0) & 0xFFu) / 255.0f;
    outColor = vec4(red, green, blue, alpha);
    gl_Position = vec4(
        Display.Translation + inPosition * Display.Size,
        0,
        1.0);
}