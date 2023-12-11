struct Cluster {
    VertexBuffer vertices;
    IndexBuffer indices;
    uint8_t triangleCount;
    uint32_t lod;
    mat4 transform;
};

struct ClusterInstance {
    uint32_t clusterID;
    uint32_t materialIndex;
    uint32_t instanceDataIndex;
};