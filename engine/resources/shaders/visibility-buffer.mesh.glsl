#extension GL_EXT_nonuniform_qualifier : enable
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
#include <includes/math.glsl>
#include <draw_data.glsl>
#include <includes/visibility-buffer-common.glsl>
DEFINE_CAMERA_SET(1)

layout(local_size_x = MESH_WORKGROUP_SIZE) in;

// this is per workgroup!! not per shader invocation!
layout(max_vertices=64, max_primitives=128) out;
layout(triangles) out;

taskPayloadSharedEXT VisibilityPayload IN;

layout(location=0) out vec4 outNDCPosition[];
layout(location=1) out flat uint outClusterInstanceID[];

uint8_t Flags_OutputTriangleCount = uint8_t(1);

layout(push_constant) uniform PushConstant {
    uint maxCluster;
    uint lodSelectionMode; // 0= screen size based, 1= force specific LOD
    float lodErrorThreshold; // screen size threshold
    uint forcedLOD; // lod to force

    float screenHeight;
    uint8_t flags;
} push;

layout(set = 0, binding = 0, scalar) buffer ClusterRef {
    Cluster clusters[];
};

layout(set = 0, binding = 1, scalar) buffer ClusterInstanceRef {
    ClusterInstance instances[];
};

layout(set = 0, binding = 2, std430) buffer ModelDataRef {
    ClusterBasedModelData modelData[];
};

// slot 3 is output image

layout(set = 0, binding = 4, scalar) buffer Statistics {
    uint totalTriangleCount;
} stats;

void main() {
    uint instanceID = IN.clusterInstanceIDs[gl_WorkGroupID.x % TASK_WORKGROUP_SIZE];

    #define instance instances[instanceID]

    uint clusterID = instance.clusterID;
    #define cluster clusters[clusterID]

    uint modelDataIndex = instance.instanceDataIndex;
    mat4 modelview = cbo.view * modelData[modelDataIndex].instanceData.transform * clusters[clusterID].transform;

    SetMeshOutputsEXT(cluster.vertexCount, cluster.triangleCount);

    const mat4 viewProj = cbo.jitteredProjection * modelview;
    for(uint vertexIndex = gl_LocalInvocationIndex; vertexIndex < cluster.vertexCount; vertexIndex += MESH_WORKGROUP_SIZE) {
        const vec4 ndcPosition = viewProj * cluster.vertices.v[vertexIndex].pos;
        gl_MeshVerticesEXT[vertexIndex].gl_Position = ndcPosition;
        outNDCPosition[vertexIndex] = ndcPosition;
        outClusterInstanceID[vertexIndex] = instanceID;
    }

    for(uint triangleIndex = gl_LocalInvocationIndex; triangleIndex < cluster.triangleCount; triangleIndex += MESH_WORKGROUP_SIZE) {
        uvec3 indices = uvec3(cluster.indices.i[triangleIndex * 3 + 0],
                              cluster.indices.i[triangleIndex * 3 + 1],
                              cluster.indices.i[triangleIndex * 3 + 2]);
        gl_PrimitiveTriangleIndicesEXT[triangleIndex] = indices;
        gl_MeshPrimitivesEXT[triangleIndex].gl_PrimitiveID = int(triangleIndex);
    }

    if(gl_LocalInvocationIndex == 0 && (push.flags & Flags_OutputTriangleCount) != 0) {
        atomicAdd(stats.totalTriangleCount, cluster.triangleCount);
    }
}