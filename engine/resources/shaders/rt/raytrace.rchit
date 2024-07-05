#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

// TODO: remove

#include "raytrace.common.glsl"

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(location = 0) rayPayloadInEXT hitPayload payload;
layout(location = 1) rayPayloadEXT bool isShadowed;

layout(binding = 1, set = 1, scalar) buffer SceneDescriptionBuffer {
    SceneElement elements[];
} sceneDescription;

layout(binding = 2, set = 1) buffer VertexBuffers {
    Vertex vertices[];
} vertexBuffers[];

layout(binding = 3, set = 1) buffer IndexBuffers {
    uint indices[];
} indexBuffers[];

hitAttributeEXT vec3 attribs;

void main()
{
    payload.hitColor = vec3(0,0,0);
}
