struct Vertex {
    vec4 pos;

    vec3 color;
    float _pad0;

    vec3 normal;
    float _pad1;

    vec3 tangent;
    float _pad2;

    vec2 uv;
};

layout(buffer_reference, std430) buffer VertexBuffer {
    Vertex v[];
};

layout(buffer_reference, std430) buffer IndexBuffer {
    uint i[];
};

struct Geometry {
    VertexBuffer vertexBuffer;
    IndexBuffer indexBuffer;
};

struct Instance {
    uint firstGeometryIndex;
    uint materialIndex;
};