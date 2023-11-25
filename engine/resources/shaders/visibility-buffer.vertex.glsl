#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : require

#include <includes/camera.glsl>
#include <includes/buffers.glsl>
#include <draw_data.glsl>
DEFINE_CAMERA_SET(1)
DEFINE_PER_DRAW_BUFFER(2)

// Per instance
layout(location = 0) in vec4 inInstanceColor;
layout(location = 1) in uvec4 inInstanceUUID;
layout(location = 2) in mat4 inInstanceTransform;

layout(location = 0) out vec4 ndcPosition;
layout(location = 1) out flat int drawID;
layout(location = 2) out flat int debugInt;

struct Cluster {
    VertexBuffer vertices;
    IndexBuffer indices;
    uint8_t indexCount;
};

layout(set = 0, binding = 0, scalar) buffer ClusterRef {
    Cluster clusters[];
};

void main() {
    drawID = gl_DrawID;

    DrawData instanceDrawData = perDrawData.drawData[perDrawDataOffsets.offset + drawID];
    uint clusterID = instanceDrawData.uuid0;
    debugInt = int(clusterID);
    Vertex vertex = clusters[clusterID].vertices.v[clusters[clusterID].indices.i[gl_VertexIndex]];

    mat4 modelview = cbo.view * inInstanceTransform;

#if 1
    vec4 viewPosition = modelview * vertex.pos;
#else
    vec3 p = vec3(0);
    switch(gl_VertexIndex % 3) {
        case 0:
            p = vec3(0);
            break;
        case 1:
            p = vec3(1,0,0);
            break;
        case 2:
            p = vec3(1,0,1);
            break;
    }
    vec4 viewPosition = modelview * vec4(p, 1);
#endif

    ndcPosition = cbo.jitteredProjection * viewPosition;
    gl_Position = ndcPosition;
}