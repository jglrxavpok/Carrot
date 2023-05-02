struct Vertex {
    vec4 pos;

    vec3 color;

    vec3 normal;

    vec4 tangent;

    vec2 uv;
};

layout(buffer_reference, std140) buffer VertexBuffer {
    Vertex v[];
};

layout(buffer_reference, scalar) buffer IndexBuffer {
    uint i[];
};

struct Geometry {
    VertexBuffer vertexBuffer;
    IndexBuffer indexBuffer;
    uint materialIndex;

    uint _pad;
};

struct Instance {
    vec4 instanceColor;
    uint firstGeometryIndex;
};