#version 450
#extension GL_ARB_separate_shader_objects : enable

const uint MAX_KEYFRAMES = 140;
const uint MAX_BONES = 20;

layout(set = 0, binding = 0) uniform CameraBufferObject {
    mat4 projection;
    mat4 view;
} cbo;

// Per vertex
layout(location = 0) in vec3 inPosition;
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

struct Keyframe {
    mat4 boneTransforms[MAX_BONES];
    float timestamp;
};

struct Animation {
    uint keyframeCount;
    float duration;
    Keyframe keyframes[MAX_KEYFRAMES];
};

layout(set = 1, binding = 0) buffer Animations {
   Animation animations[];
};

mat4 computeSkinning() {
    if(boneIDs.x < 0)
        return mat4(1.0);
    // TODO: interpolation
    float timestamp = mod(animationTime, animations[animationIndex].duration);
    uint keyframeIndex = 0;
    for(uint i = 0; i < animations[animationIndex].keyframeCount-1; i++) {
        if(timestamp <= animations[animationIndex].keyframes[i+1].timestamp) {
            keyframeIndex = i;
            break;
        }
    }
    #define keyframe animations[animationIndex].keyframes[keyframeIndex]
    mat4 boneTransform = keyframe.boneTransforms[boneIDs.x] * boneWeights.x
                        + keyframe.boneTransforms[boneIDs.y] * boneWeights.y
                        + keyframe.boneTransforms[boneIDs.z] * boneWeights.z
                        + keyframe.boneTransforms[boneIDs.w] * boneWeights.w
    ;
    return boneTransform;
}

void main() {
    mat4 skinning = computeSkinning();
    uv = inUV;
    vec4 transformedVertex = skinning * vec4(inPosition, 1.0);
    gl_Position = cbo.projection * cbo.view * inInstanceTransform * transformedVertex;

    fragColor = inColor;
    instanceColor = inInstanceColor;
}