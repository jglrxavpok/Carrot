//! Applies animations to a batch of models (all models are expected to use the same original vertex buffer)

#extension GL_EXT_scalar_block_layout : enable

// vertex
layout (local_size_x = 128) in;
// instance
layout (local_size_y = 8) in;

layout(constant_id = 0) const uint VERTEX_COUNT = 1;
layout(constant_id = 1) const uint INSTANCE_COUNT = 1;

const uint MAX_KEYFRAMES = 140;
const uint MAX_BONES = 40;

struct VertexWithBones {
    vec4 pos;
    vec3 color;
    vec3 normal;
    vec3 tangent;
    vec2 uv;
    ivec4 boneIDs;
    vec4 boneWeights;
};

struct Vertex {
    vec4 pos;
    vec3 color;
    vec3 normal;
    vec3 tangent;
    vec2 uv;
};

struct Instance {
    vec4 inInstanceColor;
    uvec4 inUUID;
    mat4 inInstanceTransform;
    mat4 inLastFrameInstanceTransform;
    uint animationIndex;
    double animationTime;
};

struct Keyframe {
    mat4 boneTransforms[MAX_BONES];
    float timestamp;
};

struct Animation {
    int keyframeCount;
    float duration;
    Keyframe keyframes[MAX_KEYFRAMES];
};

layout(set = 0, binding = 0) buffer VertexBuffer {
    VertexWithBones originalVertices[VERTEX_COUNT];
};

layout(set = 0, binding = 1) buffer InstanceBuffer {
    Instance instances[INSTANCE_COUNT];
};

layout(set = 0, binding = 2) buffer OutputVertexBuffer {
    Vertex outputVertices[VERTEX_COUNT*INSTANCE_COUNT];
};

layout(set = 1, binding = 0) buffer Animations {
    Animation animations[];
};

mat4 computeSkinning(uint instanceIndex, uint vertexIndex) {
    #define vertex originalVertices[vertexIndex]
    #define instance instances[instanceIndex]
    if(vertex.boneIDs.x < 0)
        return mat4(1.0);
    float timestamp = float(mod(instance.animationTime, animations[instance.animationIndex].duration));
    uint keyframeIndex = 0;
    for(int i = 0; i < animations[instance.animationIndex].keyframeCount-1; i++) {
        if(timestamp <= animations[instance.animationIndex].keyframes[i+1].timestamp) {
            keyframeIndex = i;
            break;
        }
    }
    uint nextKeyframeIndex = (keyframeIndex + 1) % animations[instance.animationIndex].keyframeCount;
    #define keyframe animations[instance.animationIndex].keyframes[keyframeIndex]
    #define nextKeyframe animations[instance.animationIndex].keyframes[nextKeyframeIndex]
    float timeBetweenKeyFrames = nextKeyframe.timestamp - keyframe.timestamp;
    float invAlpha = (timestamp - keyframe.timestamp) / timeBetweenKeyFrames;
    float alpha = 1 - invAlpha;
    mat4 boneTransform =
                        + keyframe.boneTransforms[vertex.boneIDs.x] * vertex.boneWeights.x * alpha
                        + keyframe.boneTransforms[vertex.boneIDs.y] * vertex.boneWeights.y * alpha
                        + keyframe.boneTransforms[vertex.boneIDs.z] * vertex.boneWeights.z * alpha
                        + keyframe.boneTransforms[vertex.boneIDs.w] * vertex.boneWeights.w * alpha

                        + nextKeyframe.boneTransforms[vertex.boneIDs.x] * vertex.boneWeights.x * invAlpha
                        + nextKeyframe.boneTransforms[vertex.boneIDs.y] * vertex.boneWeights.y * invAlpha
                        + nextKeyframe.boneTransforms[vertex.boneIDs.z] * vertex.boneWeights.z * invAlpha
                        + nextKeyframe.boneTransforms[vertex.boneIDs.w] * vertex.boneWeights.w * invAlpha
    ;
    return boneTransform;
}

void main() {
    uint vertexIndex = gl_GlobalInvocationID.x;
    uint instanceIndex = gl_GlobalInvocationID.y;

    if(vertexIndex >= VERTEX_COUNT) return;
    if(instanceIndex >= INSTANCE_COUNT) return;

    mat4 skinning = computeSkinning(instanceIndex, vertexIndex);
    uint finalVertexIndex = instanceIndex * VERTEX_COUNT + vertexIndex;
    outputVertices[finalVertexIndex].uv = originalVertices[vertexIndex].uv;
    outputVertices[finalVertexIndex].color = originalVertices[vertexIndex].color;

    vec4 skinningResult = skinning * originalVertices[vertexIndex].pos;
    outputVertices[finalVertexIndex].pos = vec4(skinningResult.xyz / skinningResult.w, 1.0);

    vec3 transformedNormal = normalize((skinning * vec4(originalVertices[vertexIndex].normal, 0.0)).xyz);
    vec3 transformedTangent = normalize((skinning * vec4(originalVertices[vertexIndex].tangent, 0.0)).xyz);
    outputVertices[finalVertexIndex].normal = transformedNormal;
    outputVertices[finalVertexIndex].tangent = transformedTangent;
}