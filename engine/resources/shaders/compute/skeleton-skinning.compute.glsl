//! Shader responsible for applying a manually modified skeleton to a given model

#extension GL_EXT_scalar_block_layout : enable

// vertex
layout (local_size_x = 128) in;
// instance
layout (local_size_y = 8) in;

layout(constant_id = 0) const uint VERTEX_COUNT = 1;

const uint MAX_BONES = 20;

struct VertexWithBones {
    vec4 pos;
    vec3 color;
    vec3 normal;
    vec2 uv;
    vec3 tangent;
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

layout(set = 0, binding = 0) buffer VertexBuffer {
    VertexWithBones originalVertices[VERTEX_COUNT];
};

layout(set = 0, binding = 1) buffer OutputVertexBuffer {
    Vertex outputVertices[VERTEX_COUNT];
};

layout(set = 1, binding = 0) buffer SkeletonBuffer {
    mat4 boneTransforms[MAX_BONES];
};

mat4 computeSkinning(uint vertexIndex) {
    #define vertex originalVertices[vertexIndex]
    if(vertex.boneIDs.x < 0)
        return mat4(1.0);
    mat4 boneTransform =
                        + boneTransforms[vertex.boneIDs.x] * vertex.boneWeights.x
                        + boneTransforms[vertex.boneIDs.y] * vertex.boneWeights.y
                        + boneTransforms[vertex.boneIDs.z] * vertex.boneWeights.z
                        + boneTransforms[vertex.boneIDs.w] * vertex.boneWeights.w

                        + boneTransforms[vertex.boneIDs.x] * vertex.boneWeights.x
                        + boneTransforms[vertex.boneIDs.y] * vertex.boneWeights.y
                        + boneTransforms[vertex.boneIDs.z] * vertex.boneWeights.z
                        + boneTransforms[vertex.boneIDs.w] * vertex.boneWeights.w
    ;
    return boneTransform;
}

void main() {
    uint vertexIndex = gl_GlobalInvocationID.x;

    if(vertexIndex >= VERTEX_COUNT) return;

    mat4 skinning = computeSkinning(vertexIndex);
    uint finalVertexIndex = vertexIndex;
    outputVertices[finalVertexIndex].pos = originalVertices[vertexIndex].pos;
    outputVertices[finalVertexIndex].normal = originalVertices[vertexIndex].normal;
    outputVertices[finalVertexIndex].uv = originalVertices[vertexIndex].uv;
    outputVertices[finalVertexIndex].color = originalVertices[vertexIndex].color;
    outputVertices[finalVertexIndex].pos = skinning * outputVertices[finalVertexIndex].pos;

    outputVertices[finalVertexIndex].pos = vec4(outputVertices[finalVertexIndex].pos.xyz / outputVertices[finalVertexIndex].pos.w, 1.0);

    vec4 transformedNormal = skinning * vec4(outputVertices[finalVertexIndex].normal, 0.0);
    outputVertices[finalVertexIndex].normal = transformedNormal.xyz;
}