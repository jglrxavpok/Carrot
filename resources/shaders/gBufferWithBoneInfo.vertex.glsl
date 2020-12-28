#version 450
#extension GL_ARB_separate_shader_objects : enable

const uint MAX_KEYFRAMES = 140;
const uint MAX_BONES = 20;

layout(set = 0, binding = 0) uniform CameraBufferObject {
    mat4 projection;
    mat4 view;
} cbo;

// Per vertex
layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;
layout(location = 3) in ivec4 boneIDs;
layout(location = 4) in vec4 boneWeights;

// Per instance
layout(location = 5) in vec4 inInstanceColor;
layout(location = 6) in mat4 inInstanceTransform;
layout(location = 10) in uint animationIndex;
layout(location = 11) in float animationTime;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 uv;
layout(location = 2) out vec4 instanceColor;

void main() {
    uv = inUV;
    gl_Position = cbo.projection * cbo.view * inInstanceTransform * inPosition;

    fragColor = inColor;
    instanceColor = inInstanceColor;
}