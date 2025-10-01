import modules.base;
import modules.vertex_buffers;

struct PushConstants {
    uint vertexCount;
    uint instanceCount;
}

struct AnimatedInstance {
    float4 color;
    uint4 uuid;
    float4x4 transform;
    float4x4 lastFrameTransform;
    uint animationIndex;
    double animationTime;
    uint8_t raytraced; // unused in skinning
}

struct Animation {
    int keyframeCount;
    float duration;   
}

struct VertexData {
    StructuredBuffer<VertexWithBones> originalVertices;
    StructuredBuffer<AnimatedInstance> instances;
    RWStructuredBuffer<Vertex> outputVertices;
}

struct AnimationData {
    StructuredBuffer<Animation> animations;
    ConstantBuffer<RWTexture2D<float4>[]> boneTextures;
}

ParameterBlock<VertexData> vertexData;
ParameterBlock<AnimationData> animationData;

float loadKeyframeTimestamp(uint animationIndex, uint keyframeIndex) {
    return keyframeIndex * animationData.animations[animationIndex].duration / animationData.animations[animationIndex].keyframeCount;
}

float4x4 loadBoneTransform(uint animationIndex, uint keyframeIndex, int boneIndex) {
    float4 row0 = animationData.boneTextures[animationIndex].Load(int2(keyframeIndex, boneIndex * 3 + 0));
    float4 row1 = animationData.boneTextures[animationIndex].Load(int2(keyframeIndex, boneIndex * 3 + 1));
    float4 row2 = animationData.boneTextures[animationIndex].Load(int2(keyframeIndex, boneIndex * 3 + 2));
    float4 row3 = float4(0, 0, 0, 1);

    // Slang is per-column
    return transpose(mat4(row0, row1, row2, row3));
}

float4x4 computeSkinning(uint instanceIndex, uint vertexIndex) {
    const VertexWithBones vertex = vertexData.originalVertices[vertexIndex];
    const AnimatedInstance instance = vertexData.instances[instanceIndex];
    if(vertex.boneIDs.x < 0) {
        return IdentityMatrix;
    }

    // TODO: perf - need some help from CPU to avoid recomputing it for each vertex!
    const Animation currentAnimation = animationData.animations[instance.animationIndex];
    float timestamp = float(fmod(instance.animationTime, currentAnimation.duration));
    uint keyframeIndex = 0;

    for(int animationIndex = 0; animationIndex < currentAnimation.keyframeCount -1; animationIndex++) {
        // TODO: binary search?
        if(timestamp <= loadKeyframeTimestamp(instance.animationIndex, animationIndex)) {
            break;
        }

        keyframeIndex++;
    }
    timestamp = loadKeyframeTimestamp(instance.animationIndex, keyframeIndex);

    const uint nextKeyframeIndex = (keyframeIndex + 1) % currentAnimation.keyframeCount;
    const float currentKeyframeTimestamp = loadKeyframeTimestamp(instance.animationIndex, keyframeIndex);
    const float timeBetweenKeyFrames = loadKeyframeTimestamp(instance.animationIndex, nextKeyframeIndex) - currentKeyframeTimestamp;
    const float invAlpha = (timestamp - currentKeyframeTimestamp) / timeBetweenKeyFrames;
    const float alpha = 1.0f - invAlpha;

    float4x4 boneTransform =
                        + loadBoneTransform(instance.animationIndex, keyframeIndex, vertex.boneIDs.x) * vertex.boneWeights.x * alpha
                        + loadBoneTransform(instance.animationIndex, keyframeIndex, vertex.boneIDs.y) * vertex.boneWeights.y * alpha
                        + loadBoneTransform(instance.animationIndex, keyframeIndex, vertex.boneIDs.z) * vertex.boneWeights.z * alpha
                        + loadBoneTransform(instance.animationIndex, keyframeIndex, vertex.boneIDs.w) * vertex.boneWeights.w * alpha

                        + loadBoneTransform(instance.animationIndex, nextKeyframeIndex, vertex.boneIDs.x) * vertex.boneWeights.x * invAlpha
                        + loadBoneTransform(instance.animationIndex, nextKeyframeIndex, vertex.boneIDs.y) * vertex.boneWeights.y * invAlpha
                        + loadBoneTransform(instance.animationIndex, nextKeyframeIndex, vertex.boneIDs.z) * vertex.boneWeights.z * invAlpha
                        + loadBoneTransform(instance.animationIndex, nextKeyframeIndex, vertex.boneIDs.w) * vertex.boneWeights.w * invAlpha
    ;
    return boneTransform;
}

[shader("compute")]
[numthreads(128, 8, 1)]
void main(uniform PushConstants push,
    uint3 globalInvocationID : SV_DispatchThreadID) {

    const uint vertexIndex = globalInvocationID.x;
    const uint instanceIndex = globalInvocationID.y;
    if(vertexIndex >= push.vertexCount) {
        return;
    }

    if(instanceIndex >= push.instanceCount) {
        return;
    }

    const float4x4 skinning = computeSkinning(instanceIndex, vertexIndex);
    const uint finalVertexIndex = instanceIndex * push.vertexCount + vertexIndex;

    StructuredBuffer<VertexWithBones> input = vertexData.originalVertices;
    RWStructuredBuffer<Vertex> output = vertexData.outputVertices;
    output[finalVertexIndex].uv = input[vertexIndex].vertex.uv;
    output[finalVertexIndex].color = input[vertexIndex].vertex.color;

    const float4 skinningResult = skinning * input[vertexIndex].vertex.pos;
    output[finalVertexIndex].pos = float4(skinningResult.xyz / skinningResult.w, 1.0f);

    const float3 transformedNormal = normalize((skinning * float4(input[vertexIndex].vertex.normal, 0)).xyz);
    const float4 transformedTangent = float4(normalize((skinning * float4(input[vertexIndex].vertex.tangent.xyz, 0)).xyz), input[vertexIndex].vertex.tangent.w);
    output[finalVertexIndex].normal = transformedNormal;
    output[finalVertexIndex].tangent = transformedTangent;
}