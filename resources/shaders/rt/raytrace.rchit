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

layout(binding = 2, set = 1) buffer VertexBuffers {
    Vertex vertices[];
} vertexBuffers[];

layout(binding = 3, set = 1) buffer IndexBuffers {
    uint indices[];
} indexBuffers[];

struct Light {
    vec3 position;
    float intensity;
    vec3 direction;
    uint type;
    vec3 color;
    bool enabled;
};

layout(binding = 4, set = 1) buffer Lights {
    vec3 ambientColor;
    uint count;
    Light l[];
} lights;

hitAttributeEXT vec3 attribs;

void main()
{
    #define sceneElement sceneDescription.elements[gl_InstanceCustomIndexEXT]
    #define indexBuffer indexBuffers[nonuniformEXT(sceneElement.mappedIndex)]
    #define vertexBuffer vertexBuffers[nonuniformEXT(sceneElement.mappedIndex)]

    ivec3 ind = ivec3(indexBuffer.indices[3*gl_PrimitiveID + 0], indexBuffer.indices[3*gl_PrimitiveID + 1], indexBuffer.indices[3*gl_PrimitiveID + 2]);
    #define v0 vertexBuffer.vertices[ind.x]
    #define v1 vertexBuffer.vertices[ind.y]
    #define v2 vertexBuffer.vertices[ind.z]
    const vec3 barycentrics = vec3(1.0-attribs.x-attribs.y, attribs.x, attribs.y);

    // TODO: vvv configurable vvv
    uint rayFlags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;
    float tMin = 0.001;
    float tMax = 1000.0;
    // TODO: ^^^ configurable ^^^

    vec4 p = (v0.pos * barycentrics.x + v1.pos * barycentrics.y + v2.pos * barycentrics.z);
    // gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;

    #define DEBUG 0
    #if DEBUG
    vec3 worldPos = (sceneElement.transform * vec4(p.xyz, 1.0)).xyz;
    vec3 rayHitPosition = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    vec3 finalColor = vec3(length(worldPos - rayHitPosition)*20);
    #else
    vec3 worldPos = (sceneElement.transform * vec4(p.xyz, 1.0)).xyz;
    vec3 normal = normalize((sceneElement.transformIT * vec4(v0.normal * barycentrics.x + v1.normal * barycentrics.y + v2.normal * barycentrics.z, 0.0))).xyz;

    vec3 finalColor = vec3(1.0);

    vec3 lightContribution = vec3(0.0);

    for(uint i = 0; i < lights.count; i++) {
        #define light lights.l[i]

        if(!light.enabled)
            continue;

        // point type
        vec3 lightDirection;

        if(light.type == 0) {
            vec3 lightPosition = light.position;
            lightDirection = normalize(lightPosition-worldPos);
        } else if(light.type == 1) {
            lightDirection = -normalize(light.direction);
        } else {
            // TODO
            lightDirection = vec3(1.0);
        }

        // is this point in shadow?
        isShadowed = true;
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

            if(!isShadowed) {
                // TODO: proper lighting model

                float lightFactor = 1.0f;
                if(light.type == 0) {
                    const float maxDist = 3;
                    float distance = length(light.position-worldPos);
                    if(distance < maxDist) {
                        lightFactor = (maxDist-distance)/maxDist; // TODO: calculate based on intensity, falloff, etc.
                    } else {
                        lightFactor = 0.0f;
                    }
                }
                lightContribution += light.color * lightFactor * light.intensity;
            }
        }
    }

    lightContribution = min(vec3(1.0)-lights.ambientColor, lightContribution);

    finalColor = lightContribution + lights.ambientColor;
    #endif
    payload.hitColor = finalColor;
}
