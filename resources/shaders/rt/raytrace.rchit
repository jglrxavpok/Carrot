#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "raytrace.common.glsl"

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(location = 0) rayPayloadInEXT hitPayload payload;
layout(location = 1) rayPayloadEXT bool isShadowed;

layout(binding = 1, set = 1) buffer SceneDescriptionBuffer {
    SceneElement elements[];
} sceneDescription;

layout(binding = 2, set = 1) buffer VertexBuffers {
    Vertex vertices[];
} vertexBuffers[];

layout(binding = 3, set = 1, scalar) buffer IndexBuffers {
    uint indices[];
} indexBuffers[];

hitAttributeEXT vec3 attribs;

void main()
{
    // TODO
    const vec3 lightPosition = vec3(2,2,1);

    #define sceneElement sceneDescription.elements[gl_InstanceCustomIndexEXT]
    #define indexBuffer indexBuffers[nonuniformEXT(sceneElement.mappedIndex)]
    #define vertexBuffer vertexBuffers[nonuniformEXT(sceneElement.mappedIndex)]

    ivec3 ind = ivec3(indexBuffer.indices[3*gl_PrimitiveID + 0], indexBuffer.indices[3*gl_PrimitiveID + 1], indexBuffer.indices[3*gl_PrimitiveID + 2]);
    #define v0 vertexBuffer.vertices[ind.x]
    #define v1 vertexBuffer.vertices[ind.y]
    #define v2 vertexBuffer.vertices[ind.z]
    const vec3 barycentrics = vec3(1.0-attribs.x-attribs.y, attribs.x, attribs.y);

    uint id = sceneElement.mappedIndex % 3;

    // TODO: vvv configurable vvv
    uint rayFlags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;
    float tMin = 0.001;
    float tMax = 1000.0;
    // TODO: ^^^ configurable ^^^

    // TODO: use vertex buffer
    vec3 worldPos = (sceneElement.transform * (v0.pos * barycentrics.x + v1.pos * barycentrics.y + v2.pos * barycentrics.z)).xyz;
    vec3 normal = normalize((sceneElement.transform * vec4(v0.normal * barycentrics.x + v1.normal * barycentrics.y + v2.normal * barycentrics.z, 0.0)).xyz);

    vec3 finalColor = vec3(1.0);

    isShadowed = true;

    vec3 lightDirection = normalize(lightPosition-worldPos);
    if(dot(normal, lightDirection) > 0) {
        traceRayEXT(topLevelAS, // AS
            rayFlags, // ray flags
            0xFF, // cull mask
            0, // sbtRecordOffset
            0, // sbtRecordStride
            1, // missIndex
            worldPos, // ray origin
            tMin, // ray min range
            lightDirection, // ray direction
            tMax, // ray max range,
            1 // payload location
        );
    }

    //isShadowed = false; // TODO: debug only

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
