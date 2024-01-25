struct Cluster {
    VertexBuffer vertices;
    IndexBuffer indices;
    uint8_t triangleCount;
    uint32_t lod;
    mat4 transform;
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