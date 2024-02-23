#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int32 : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_mesh_shader : require

#include <includes/camera.glsl>
#include <includes/buffers.glsl>
#include <includes/clusters.glsl>
#include <draw_data.glsl>
DEFINE_CAMERA_SET(1)

// TODO: change workgroup size
const uint WORKGROUP_SIZE = 1;

layout(local_size_x=WORKGROUP_SIZE) in;
layout(max_vertices=128, max_primitives=128) out;
layout(triangles) out;

layout(location=0) out vec4 outNDCPosition[];
layout(location=1) out flat uint outClusterInstanceID[];

layout(push_constant) uniform PushConstant {
    uint maxCluster;
} push;

layout(set = 0, binding = 0, scalar) buffer ClusterRef {
    Cluster clusters[];
};

layout(set = 0, binding = 1, scalar) buffer ClusterInstanceRef {
    ClusterInstance instances[];
};

layout(set = 0, binding = 2, scalar) buffer ModelDataRef {
    InstanceData modelData[];
};

void main() {
    uint instanceID = gl_GlobalInvocationID.x;

    if(instanceID >= push.maxCluster) {
        SetMeshOutputsEXT(0,0);
        return;
    }

    #define instance instances[instanceID]

    uint clusterID = instance.clusterID;
    #define cluster clusters[clusterID]

    if(clusters[clusterID].lod != 1) {
        SetMeshOutputsEXT(0,0);
        return;
    }

    uint modelDataIndex = instance.instanceDataIndex;
    mat4 modelview = cbo.view * modelData[modelDataIndex].transform * clusters[clusterID].transform;

    #if 1
    SetMeshOutputsEXT(cluster.vertexCount, cluster.triangleCount);

    for(uint vertexIndex = 0; vertexIndex < cluster.vertexCount; vertexIndex++) {
        const vec4 viewPosition = modelview * cluster.vertices.v[vertexIndex].pos*5;
        const vec4 ndcPosition = cbo.jitteredProjection * viewPosition;
        gl_MeshVerticesEXT[vertexIndex].gl_Position = ndcPosition;
        outNDCPosition[vertexIndex] = ndcPosition;
        outClusterInstanceID[vertexIndex] = clusterID;
    }

    for(uint triangleIndex = 0; triangleIndex < cluster.triangleCount; triangleIndex++) {
        uvec3 indices = uvec3(cluster.indices.i[triangleIndex * 3 + 0],
                              cluster.indices.i[triangleIndex * 3 + 1],
                              cluster.indices.i[triangleIndex * 3 + 2]);
        gl_PrimitiveTriangleIndicesEXT[triangleIndex] = indices;
        gl_MeshPrimitivesEXT[triangleIndex].gl_PrimitiveID = int(triangleIndex);
    }
    #else
    SetMeshOutputsEXT(3, 1);
    for(uint vertexIndex = 0; vertexIndex < 3; vertexIndex++) {
        const vec4 viewPosition = cbo.view * vec4((vertexIndex % 2) * 5, (vertexIndex / 2)*5, 0, 1);
        const vec4 ndcPosition = cbo.jitteredProjection * viewPosition;
        gl_MeshVerticesEXT[vertexIndex].gl_Position = ndcPosition;
        outNDCPosition[vertexIndex] = ndcPosition;
        outClusterInstanceID[vertexIndex] = 0;
    }
    gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0,1,2);
    gl_MeshPrimitivesEXT[0].gl_PrimitiveID = int(1);
    #endif
}