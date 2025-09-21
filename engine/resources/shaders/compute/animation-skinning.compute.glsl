//! Applies animations to a batch of models (all models are expected to use the same original vertex buffer)

#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : enable

// vertex
layout (local_size_x = 128) in;
// instance
layout (local_size_y = 8) in;

const uint MAX_KEYFRAMES = 140;
const uint MAX_BONES = 40;

struct VertexWithBones {
    vec4 pos;
    vec3 color;
    vec3 normal;
    vec4 tangent;
    vec2 uv;
    vec4 boneWeights;
    u8vec4 boneIDs;
};

struct Vertex {
    vec4 pos;
    vec3 color;
    vec3 normal;
    vec4 tangent;
    vec2 uv;
};

struct Instance {
    vec4 inInstanceColor;
    uvec4 inUUID;
    mat4 inInstanceTransform;
    mat4 inLastFrameInstanceTransform;
    uint animationIndex;
    double animationTime;
    uint8_t raytraced; // unused in skinning
};

struct Animation {
    int keyframeCount;
    float duration;
};

layout(set = 0, binding = 0) buffer VertexBuffer {
    VertexWithBones originalVertices[];
};

layout(set = 0, binding = 1) buffer InstanceBuffer {
    Instance instances[];
};

layout(set = 0, binding = 2) buffer OutputVertexBuffer {
    Vertex outputVertices[];
};

layout(set = 1, binding = 0) buffer Animations {
    Animation animations[];
};

layout(push_constant) uniform PushConstant {
    uint vertexCount;
    uint instanceCount;
} push;

// indexed by animation index
// X is keyframe index, Y/3 is bone index
layout(set = 1, binding = 1, rgba32f) uniform readonly image2D animationDataBoneTextures[];

float loadKeyframeTimestamp(uint animationIndex, uint keyframeIndex) {
    return keyframeIndex * animations[animationIndex].duration / animations[animationIndex].keyframeCount;
}

mat4 loadBoneTransform(uint animationIndex, uint keyframeIndex, int boneIndex) {
    vec4 row0 = imageLoad(animationDataBoneTextures[animationIndex], ivec2(keyframeIndex, boneIndex * 3 + 0));
    vec4 row1 = imageLoad(animationDataBoneTextures[animationIndex], ivec2(keyframeIndex, boneIndex * 3 + 1));
    vec4 row2 = imageLoad(animationDataBoneTextures[animationIndex], ivec2(keyframeIndex, boneIndex * 3 + 2));
    vec4 row3 = vec4(0, 0, 0, 1);

    // GLSL is per-column
    return transpose(mat4(row0, row1, row2, row3));
}

mat4 computeSkinning(uint instanceIndex, uint vertexIndex) {
    #define vertex originalVertices[vertexIndex]
    #define instance instances[instanceIndex]
    if(vertex.boneIDs.x < 0)
        return mat4(1.0);

    // TODO: perf - need some help from CPU to avoid recomputing it for each vertex!
    float timestamp = float(mod(instance.animationTime, animations[instance.animationIndex].duration));
    uint keyframeIndex = 0;

    for(int i = 0; i < animations[instance.animationIndex].keyframeCount-1; i++) {
        if(timestamp <= loadKeyframeTimestamp(instance.animationIndex, i+1)) {
            break;
        }
        keyframeIndex++;
    }
    timestamp = loadKeyframeTimestamp(instance.animationIndex, keyframeIndex);
    uint nextKeyframeIndex = (keyframeIndex + 1) % animations[instance.animationIndex].keyframeCount;
    const uint animationIndex = instance.animationIndex;
    #define keyframe animations[animationIndex].keyframes[keyframeIndex]
    #define nextKeyframe animations[animationIndex].keyframes[nextKeyframeIndex]
    const float currentKeyframeTimestamp = loadKeyframeTimestamp(animationIndex, keyframeIndex);
    const float timeBetweenKeyFrames = loadKeyframeTimestamp(animationIndex, nextKeyframeIndex) - currentKeyframeTimestamp;
    const float invAlpha = (timestamp - currentKeyframeTimestamp) / timeBetweenKeyFrames;
    const float alpha = 1.0f - invAlpha;

    mat4 boneTransform =
                        + loadBoneTransform(animationIndex, keyframeIndex, vertex.boneIDs.x) * vertex.boneWeights.x * alpha
                        + loadBoneTransform(animationIndex, keyframeIndex, vertex.boneIDs.y) * vertex.boneWeights.y * alpha
                        + loadBoneTransform(animationIndex, keyframeIndex, vertex.boneIDs.z) * vertex.boneWeights.z * alpha
                        + loadBoneTransform(animationIndex, keyframeIndex, vertex.boneIDs.w) * vertex.boneWeights.w * alpha

                        + loadBoneTransform(animationIndex, nextKeyframeIndex, vertex.boneIDs.x) * vertex.boneWeights.x * invAlpha
                        + loadBoneTransform(animationIndex, nextKeyframeIndex, vertex.boneIDs.y) * vertex.boneWeights.y * invAlpha
                        + loadBoneTransform(animationIndex, nextKeyframeIndex, vertex.boneIDs.z) * vertex.boneWeights.z * invAlpha
                        + loadBoneTransform(animationIndex, nextKeyframeIndex, vertex.boneIDs.w) * vertex.boneWeights.w * invAlpha
    ;
    return boneTransform;
}

void main() {
    uint vertexIndex = gl_GlobalInvocationID.x;
    uint instanceIndex = gl_GlobalInvocationID.y;

    if(vertexIndex >= push.vertexCount) return;
    if(instanceIndex >= push.instanceCount) return;

    mat4 skinning = computeSkinning(instanceIndex, vertexIndex);
    uint finalVertexIndex = instanceIndex * push.vertexCount + vertexIndex;
    outputVertices[finalVertexIndex].uv = originalVertices[vertexIndex].uv;
    outputVertices[finalVertexIndex].color = originalVertices[vertexIndex].color;

    vec4 skinningResult = skinning * originalVertices[vertexIndex].pos;
    outputVertices[finalVertexIndex].pos = vec4(skinningResult.xyz / skinningResult.w, 1.0);

    vec3 transformedNormal = normalize((skinning * vec4(originalVertices[vertexIndex].normal, 0.0)).xyz);
    vec4 transformedTangent = vec4(normalize((skinning * vec4(originalVertices[vertexIndex].tangent.xyz, 0.0)).xyz), originalVertices[vertexIndex].tangent.w);
    outputVertices[finalVertexIndex].normal = transformedNormal;
    outputVertices[finalVertexIndex].tangent = transformedTangent;
}