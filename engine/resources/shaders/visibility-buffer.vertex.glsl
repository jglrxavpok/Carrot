#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int32 : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : require

#include <includes/camera.glsl>
#include <includes/buffers.glsl>
#include <includes/clusters.glsl>
#include <draw_data.glsl>
DEFINE_CAMERA_SET(1)
DEFINE_PER_DRAW_BUFFER(2)

// Per instance
layout(location = 0) in vec4 inInstanceColor;
layout(location = 1) in uvec4 inInstanceUUID;
layout(location = 2) in mat4 inInstanceTransform;

layout(location = 0) out vec4 ndcPosition;
layout(location = 1) out flat uint instanceID;


layout(set = 0, binding = 0, scalar) buffer ClusterRef {
    Cluster clusters[];
};

layout(set = 0, binding = 1, scalar) buffer ClusterInstanceRef {
    ClusterInstance instances[];
};

void main() {
    uint drawID = gl_DrawID;

    DrawData instanceDrawData = perDrawData.drawData[perDrawDataOffsets.offset + drawID];
    instanceID = instanceDrawData.uuid0;
    uint clusterID = instances[instanceID].clusterID;
    Vertex vertex = clusters[clusterID].vertices.v[clusters[clusterID].indices.i[gl_VertexIndex]];

    mat4 modelview = cbo.view * inInstanceTransform * clusters[clusterID].transform;

    vec4 viewPosition = modelview * vertex.pos;

    ndcPosition = cbo.jitteredProjection * viewPosition;
    gl_Position = ndcPosition;
}