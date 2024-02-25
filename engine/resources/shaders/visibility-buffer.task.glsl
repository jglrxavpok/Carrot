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
DEFINE_CAMERA_SET(1)

// TODO: change workgroup size
const uint WORKGROUP_SIZE = 1;
layout(local_size_x = WORKGROUP_SIZE) in;

struct Task
{
    uint clusterInstanceID;
};

taskPayloadSharedEXT Task OUT;

layout(push_constant) uniform PushConstant {
    uint maxCluster;
    uint lodSelectionMode; // 0= screen size based, 1= force specific LOD
    float lodErrorThreshold; // screen size threshold
    uint forcedLOD; // lod to force

    float screenHeight;
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

// assume a fixed resolution and fov
const float testFOV = M_PI_OVER_2;
const float cotHalfFov = 1.0f / tan(testFOV / 2.0f);

// project given transformed (ie in view space) sphere to an error value in pixels
// xyz is center of sphere
// w is radius of sphere
float projectErrorToScreen(vec4 transformedSphere) {
    // https://stackoverflow.com/questions/21648630/radius-of-projected-sphere-in-screen-space
    if (isinf(transformedSphere.w)) {
        return transformedSphere.w;
    }
    const float d2 = dot(transformedSphere.xyz, transformedSphere.xyz);
    const float r = transformedSphere.w;
    return push.screenHeight * cotHalfFov * r / sqrt(d2 - r*r);
}

bool cull(uint clusterInstanceID) {
    // TODO: occlusion culling
    // TODO: backface culling
    // TODO: frustum culling
    uint clusterID = instances[clusterInstanceID].clusterID;
    uint modelDataIndex = instances[clusterInstanceID].instanceDataIndex;

    if(push.lodSelectionMode == 0) {
        const mat4 modelview = cbo.view * modelData[modelDataIndex].transform * clusters[clusterID].transform;
        vec4 projectedBounds = vec4(clusters[clusterID].boundingSphere.xyz, max(clusters[clusterID].error, 10e-10f));
        projectedBounds = transformSphere(projectedBounds, modelview);

        vec4 parentProjectedBounds = vec4(clusters[clusterID].parentBoundingSphere.xyz, max(clusters[clusterID].parentError, 10e-10f));
        parentProjectedBounds = transformSphere(parentProjectedBounds, modelview);

        const float clusterError = projectErrorToScreen(projectedBounds);
        const float parentError = projectErrorToScreen(parentProjectedBounds);
        const bool render = clusterError <= push.lodErrorThreshold && parentError > push.lodErrorThreshold;
        return !render;
    } else {
        return clusters[clusterID].lod != uint(push.forcedLOD);
    }
}

void main() {
    uint clusterID = gl_GlobalInvocationID.x;
    bool culled = clusterID >= push.maxCluster || cull(clusterID);

    // TODO: do multiple emits per task shader? (see NVIDIA example)
    if(!culled) {
        OUT.clusterInstanceID = clusterID;
        EmitMeshTasksEXT(1, 1, 1);
    }

}