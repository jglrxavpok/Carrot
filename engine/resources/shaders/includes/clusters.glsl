struct Cluster {
    PackedVertexBuffer vertices;
    IndexBuffer16 indices;
    uint8_t triangleCount;
    uint8_t vertexCount;
    uint8_t lod;
    mat4x3 transform;
    vec4 boundingSphere;
    vec4 parentBoundingSphere;
    float error;
    float parentError;
};

struct ClusterInstance {
    uint32_t clusterID;
    uint32_t materialIndex;
    uint32_t instanceDataIndex;
};

struct ClusterBasedModelData {
    InstanceData instanceData;
    uint8_t visible;
};

struct ClusterReadbackData {
    uint8_t visible;
};