#include "math.glsl"

struct Cluster {
    PackedVertexBuffer vertices;
    IndexBuffer16 indices;
    uint8_t triangleCount;
    uint8_t vertexCount;
    uint8_t lod;
    mat4x3 transform;
    vec4 boundingSphere;
    vec4 refinedBoundingSphere;
    float error;
    float refinedError;
};

struct ClusterInstance {
    uint32_t clusterID;
    uint32_t materialIndex;
    uint32_t instanceDataIndex;
};

struct ClusterBasedModelData {
    InstanceData instanceData;
    uint8_t visible;
    uint8_t pad[15];
};

// Needs to be multiplied by screen height after (ratio in 0..1)
float computeClusterScreenError(vec3 cameraPosition, mat4 modelMatrix, vec4 clusterBoundingSphere, float clusterError) {
    // assume a fixed resolution and fov
    const float testFOV = M_PI_OVER_2;
    const float cotHalfFov = 1.0f / tan(testFOV / 2.0f);

    float error = 0.0f;
    vec4 transformedSphere = transformSphere(clusterBoundingSphere, modelMatrix);
    vec3 dpos = transformedSphere.xyz - cameraPosition;
    float d = sqrt(dot(dpos, dpos)) - transformedSphere.w;
    error = clusterError / (d /*TODO: camera znear*/) * cotHalfFov;
    error = length((modelMatrix * vec4(error, 0,0,0)).xyz);
    return error;
}