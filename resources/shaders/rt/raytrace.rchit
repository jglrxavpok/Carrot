#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "raytrace.common.glsl"

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(location = 0) rayPayloadInEXT hitPayload payload;
layout(location = 1) rayPayloadEXT bool isShadowed;

layout(binding = 1, set = 1, scalar) buffer SceneDescriptionBuffer {
    SceneElement elements[];
} sceneDescription;

layout(binding = 2, set = 1, scalar) buffer VertexBuffers {
    Vertex vertices[];
} vertexBuffers[];

layout(binding = 3, set = 1, scalar) buffer IndexBuffers {
    uint indices[];
} indexBuffers[];

void main()
{
    // TODO
    vec3 lightPosition = vec3(-1,1,1);

    #define sceneElement sceneDescription.elements[gl_InstanceCustomIndexEXT]
    #define indexBuffer indexBuffers[sceneElement.mappedIndex]
    #define vertexBuffer vertexBuffers[sceneElement.mappedIndex]

    uint id = sceneElement.mappedIndex % 3;
    vec3 finalColor = vec3(1.0);

    // TODO: vvv configurable vvv
    uint rayFlags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;
    float tMin = 0.001;
    float tMax = 1000.0;
    // TODO: ^^^ configurable ^^^

    // TODO: use vertex buffer
    vec3 worldPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;

    isShadowed = true;

    traceRayEXT(topLevelAS, // AS
        rayFlags, // ray flags
        0xFF, // cull mask
        0, // sbtRecordOffset
        0, // sbtRecordStride
        1, // missIndex
        worldPos, // ray origin
        tMin, // ray min range
        lightPosition, // ray direction
        tMax, // ray max range,
        1 // payload location
    );

/*    finalColor.r *= (id == 0) ? 1 : 0;
    finalColor.g *= (id == 1) ? 1 : 0;
    finalColor.b *= (id == 2) ? 1 : 0;
*/
    if(isShadowed) {
        payload.hitColor = finalColor * 0.1; // TODO: ambient lighting
    } else {
        payload.hitColor = finalColor;
    }
}
