#version 450

// vertex
layout (local_size_x = 64) in;
// instance
layout (local_size_y = 8) in;
// mesh ID
layout (local_size_z = 2) in;

layout(constant_id = 0) const uint MESH_COUNT = 1;
layout(constant_id = 1) const uint INSTANCE_COUNT = 1;

const uint MAX_KEYFRAMES = 140;
const uint MAX_BONES = 20;

struct VertexWithBones {
    vec3 pos;
    vec3 color;
    vec2 uv;
    ivec4 boneIDs;
    vec4 boneWeights;
};

struct Instance {
    vec4 inInstanceColor;
    mat4 inInstanceTransform;
    uint animationIndex;
    float animationTime;
};

struct Keyframe {
    mat4 boneTransforms[MAX_BONES];
    float timestamp;
};

struct Animation {
    uint keyframeCount;
    float duration;
    Keyframe keyframes[MAX_KEYFRAMES];
};

layout(push_constant) uniform PushConstants {
    uint meshVertexCounts[];
};

layout(set = 0, binding = 1) buffer VertexBuffer {
    VertexWithBones vertices[];
};

layout(set = 0, binding = 2) buffer InstanceBuffer {
    Instance instances[];
};

layout(set = 0, binding = 3) buffer OutputVertexBuffer {
    VertexWithBones outputVertices[];
};

layout(set = 1, binding = 0) buffer Animations {
    Animation animations[];
};

mat4 computeSkinning(uint instanceIndex, uint vertexIndex) {
    #define vertex vertices[vertexIndex]
    #define instance instances[instanceIndex]
    if(vertex.boneIDs.x < 0)
        return mat4(1.0);
    // TODO: interpolation
    float timestamp = mod(instance.animationTime, animations[instance.animationIndex].duration);
    uint keyframeIndex = 0;
    for(uint i = 0; i < animations[instance.animationIndex].keyframeCount-1; i++) {
        if(timestamp <= animations[instance.animationIndex].keyframes[i+1].timestamp) {
            keyframeIndex = i;
            break;
        }
    }
    #define keyframe animations[instance.animationIndex].keyframes[keyframeIndex]
    mat4 boneTransform =
                        + keyframe.boneTransforms[vertex.boneIDs.x] * vertex.boneWeights.x
                        + keyframe.boneTransforms[vertex.boneIDs.y] * vertex.boneWeights.y
                        + keyframe.boneTransforms[vertex.boneIDs.z] * vertex.boneWeights.z
                        + keyframe.boneTransforms[vertex.boneIDs.w] * vertex.boneWeights.w
    ;
    return boneTransform;
}

void main() {
    // TODO
}