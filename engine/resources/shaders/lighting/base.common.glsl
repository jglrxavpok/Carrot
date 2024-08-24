#include <includes/math.glsl>

#ifdef HARDWARE_SUPPORTS_RAY_TRACING
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_ray_query : enable

layout(set = 6, binding = 0) uniform accelerationStructureEXT topLevelAS;
layout(set = 6, binding = 1) uniform texture2D noiseTexture;
#endif

float computePointLight(vec3 worldPos, vec3 normal, uint lightIndex) {
    #define light lights.l[lightIndex]
    vec3 lightPosition = light.v0;
    vec3 point2light = lightPosition-worldPos;

    float distance = length(point2light);
    float attenuation = light.v1.x + light.v1.y * distance + light.v1.z * distance * distance;

    float inclinaisonFactor = 1.0;//abs(dot(normal, point2light / distance));

    return max(0, 1.0f / attenuation) * inclinaisonFactor;
    #undef light
}

float computeDirectionalLight(vec3 worldPos, vec3 normal, uint lightIndex) {
    #define light lights.l[lightIndex]
    vec3 lightDirection = normalize(light.v0);
    return max(0, dot(lightDirection, normal));
    #undef light
}

float computeSpotLight(vec3 worldPos, vec3 normal, uint lightIndex) {
    #define light lights.l[lightIndex]
    float cutoffCosAngle = light.v2.x;
    float outerCosCutoffAngle = light.v2.y;

    vec3 lightPosition = light.v0;
    vec3 point2light = normalize(lightPosition-worldPos);
    vec3 lightDirection = -normalize(light.v1);
    float intensity = dot(point2light, lightDirection);
    float cutoffRange = outerCosCutoffAngle - cutoffCosAngle;
    return clamp((intensity - outerCosCutoffAngle) / cutoffRange, 0, 1);
    #undef light
}


/**
* Computes the direct contribution of the given light
*/
vec3 computeLightContribution(uint lightIndex, vec3 worldPos, vec3 normal) {
    #define light (lights.l[lightIndex])
    uint8_t type = light.type;
    float enabledF = float((light.flags & 1) != 0);

    float lightFactor = 0.0f;
    if(type == POINT_LIGHT_TYPE) {
        lightFactor = computePointLight(worldPos, normal, lightIndex);
    } else if(type == DIRECTIONAL_LIGHT_TYPE) {
        lightFactor = computeDirectionalLight(worldPos, normal, lightIndex);
    } else if(type == SPOT_LIGHT_TYPE) {
        lightFactor = computeSpotLight(worldPos, normal, lightIndex);
    }

    return enabledF * light.color * lightFactor * light.intensity;
    #undef light
}


#ifdef HARDWARE_SUPPORTS_RAY_TRACING
rayQueryEXT rayQuery;

void initRayQuery(vec3 startPos, vec3 direction, float maxDistance, float tMin) {
    // Initializes a ray query object but does not start traversal
    rayQueryInitializeEXT(rayQuery, topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT | gl_RayFlagsOpaqueEXT, 0xFF, startPos, tMin, direction, maxDistance);
}

bool traceShadowRay() {
    if(!push.hasTLAS) {
        return true;
    }

    rayQueryProceedEXT(rayQuery);

    // Returns type of committed (true) intersection
    return rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionNoneEXT;
}

vec3 getVectorToLight(uint lightIndex, vec3 worldPos) {
    #define light (lights.l[lightIndex])
    uint8_t type = light.type;
    if (type == POINT_LIGHT_TYPE || type == SPOT_LIGHT_TYPE) {
        return light.v0 - worldPos;
    } else if(type == DIRECTIONAL_LIGHT_TYPE) {
        return light.v0;
    }
    return vec3(0,0,0);
    #undef light
}

vec3 computeDirectLightingFromLights(inout RandomSampler rng, inout float lightPDF, vec3 worldPos, vec3 normal, float maxDistance) {
    vec3 lightContribution = vec3(0.0);

    if(activeLights.count <= 0) {
        lightPDF = 1.0;
        return vec3(0.0);
    }

    // TODO: Light Importance Sampling, will require denoising of result
    //    float pdfInv = activeLights.count;
    //    uint i = min(activeLights.count - 1, uint(floor(sampleNoise(rng) * activeLights.count)));
    //    i = activeLights.indices[i];
    //    lightPDF = 1.0 / pdfInv;
    lightPDF = 1.0;

    [[dont_unroll]] for (uint li = 0; li < activeLights.count; li++)
    {
        uint i = activeLights.indices[li];
        #define light (lights.l[i])

        bool enabled = (light.flags & 1) != 0;

        if(enabled)
        {
            const vec3 L = getVectorToLight(i, worldPos);
            const float enabledF = float(enabled);

            // from https://github.com/nvpro-samples/vk_raytracing_tutorial_KHR/tree/master/ray_tracing_rayquery
            float lightDistance = maxDistance;

            if(light.type != DIRECTIONAL_LIGHT_TYPE) {
                lightDistance = min(lightDistance, length(L));
            }

            // Init ray query before light contribution to hide latency
            // Ray Query for shadow
            float tMin      = 0.01f;
            initRayQuery(worldPos, normalize(L), lightDistance * enabledF, tMin);
            const vec3 singleLightContribution = computeLightContribution(i, worldPos, normal) /* cos term already in computeLightContribution */;
            const float visibility = float(traceShadowRay());

            lightContribution += enabledF * visibility * singleLightContribution;
        }
        #undef light
    }
    return lightContribution;
}
#endif