struct Vec3 {
    float x;
    float y;
    float z;
};

struct Triangle {
    u64 index;
    Vec3 a;
    Vec3 b;
    Vec3 c;
};

struct Link {
    u64 indexA;
    u64 indexB;
};

struct PortalVertex {
    u64 key;
    Vec3 left;
    Vec3 right;
};

struct PortalVertexMapEntry {
    u64 key;
    u64 vertexCount;
    PortalVertex values[vertexCount];
};

struct CNav {
    char magic[4];
    u32 version;
    
    u64 triangleCount;
    Triangle triangles[triangleCount];
    
    u64 linkCount;
    Link links[linkCount];
    
    u64 portalVertexEntryCount;
    PortalVertexMapEntry portalVertices[portalVertexEntryCount];
};

CNav nav @ 0x00;

