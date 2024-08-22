#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int32 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : require

#include <includes/camera.glsl>
#include <includes/buffers.glsl>
#include <includes/clusters.glsl>
#include <includes/math.glsl>
#include <includes/visibility-buffer-common.glsl>
DEFINE_CAMERA_SET(1)

#include <includes/frustum.glsl>

layout(local_size_x = 32) in;

layout(push_constant) uniform PushConstant {
    uint maxGroupID;
    uint lodSelectionMode; // 0= screen size based, 1= force specific LOD
    float lodErrorThreshold; // screen size threshold
    uint forcedLOD; // lod to force

    float screenHeight;

    uint64_t groupDataAddress;
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

// slot 3 used by output image
// slot 4 is stats buffer
layout(set = 0, binding = 5, scalar) buffer ActiveGroupOffsets {
    uint64_t activeGroupOffsets[];
};

layout(set = 0, binding = 6, scalar) writeonly buffer ReadbackRef {
    uint32_t visibleCount;
    uint32_t visibleGroupInstanceIndices[];
} readback;

layout(buffer_reference, scalar) buffer ActiveGroup {
    uint32_t groupIndex;
    uint8_t clusterCount;
    uint8_t pad0;
    uint8_t pad1;
    uint8_t pad2;
    uint32_t clusterInstances[];
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
    uint clusterID = instances[clusterInstanceID].clusterID;
    uint modelDataIndex = instances[clusterInstanceID].instanceDataIndex;

    if(modelData[modelDataIndex].visible == 0) {
        return true;
    }

    if(push.lodSelectionMode == 0) {
        const mat4 model = modelData[modelDataIndex].instanceData.transform * mat4(clusters[clusterID].transform);
        const mat4 modelview = cbo.view * model;

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
    uint groupIndex = gl_LocalInvocationIndex + gl_WorkGroupID.x * TASK_WORKGROUP_SIZE;
    if(groupIndex >= push.maxGroupID) {
        return;
    }

    ActiveGroup group = ActiveGroup(push.groupDataAddress + activeGroupOffsets[groupIndex]);
    for(uint clusterIndex = 0; clusterIndex < group.clusterCount; clusterIndex++) {
        uint clusterID = group.clusterInstances[clusterIndex];
        bool culled = cull(clusterID);
        if(!culled) {
            uint readbackIndex = atomicAdd(readback.visibleCount, 1);
            readback.visibleGroupInstanceIndices[nonuniformEXT(readbackIndex)] = group.groupIndex;
            return;
        }
    }
}