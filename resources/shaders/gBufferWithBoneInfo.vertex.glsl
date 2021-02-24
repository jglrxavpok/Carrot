#version 450
#extension GL_ARB_separate_shader_objects : enable

const uint MAX_KEYFRAMES = 140;
const uint MAX_BONES = 20;

layout(set = 0, binding = 0) uniform CameraBufferObject {
    mat4 projection;
    mat4 view;
    mat4 inverseView;
    mat4 inverseProjection;
} cbo;

// Per vertex
layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inUV;
layout(location = 4) in ivec4 boneIDs;
layout(location = 5) in vec4 boneWeights;

// Per instance
layout(location = 6) in vec4 inInstanceColor;
layout(location = 7) in mat4 inInstanceTransform;
layout(location = 11) in uint animationIndex;
layout(location = 12) in float animationTime;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 uv;
layout(location = 2) out vec4 instanceColor;
layout(location = 3) out vec3 outViewPos;
layout(location = 4) out vec3 outViewNormal;

void main() {
    uv = inUV;
    mat4 modelview = cbo.view * inInstanceTransform;
    vec4 viewPosition = modelview * inPosition;
    gl_Position = cbo.projection * viewPosition;

    fragColor = inColor;
    instanceColor = inInstanceColor;
    outViewPos = viewPosition.xyz/viewPosition.w;

    outViewNormal = normalize((transpose(inverse(mat3(modelview)))) * inNormal);
}