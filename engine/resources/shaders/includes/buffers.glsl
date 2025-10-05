struct Vertex {
    vec4 pos;

    vec3 color;

    vec3 normal;

    vec4 tangent;

    vec2 uv;
};

struct PackedVertex {
    vec3 pos;
    vec3 color;
    vec3 normal;
    vec4 tangent;
    vec2 uv;
};

layout(buffer_reference, std140) buffer VertexBuffer {
    Vertex v[];
};

layout(buffer_reference, scalar) buffer PackedVertexBuffer {
    PackedVertex v[];
};

layout(buffer_reference, scalar) buffer IndexBuffer {
    uint i[];
};

layout(buffer_reference, scalar) buffer IndexBuffer16 {
    uint16_t i[];
};

layout(buffer_reference, std140) buffer TransformBuffer {
    mat4x3 m;
};

#define GEOMETRY_FORMAT_DEFAULT 0
#define GEOMETRY_FORMAT_PACKED 1

struct Geometry {
    VertexBuffer vertexBuffer;
    IndexBuffer indexBuffer;
    TransformBuffer transformBuffer;
    uint materialIndex;
    uint8_t geometryFormat;
};

struct Instance {
    vec4 instanceColor;
    uint firstGeometryIndex;
};

struct InstanceData {
    vec4 color;
    uvec4 uuid;
    mat4 transform;
    mat4 lastFrameTransform;
};