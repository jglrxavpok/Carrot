#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_scalar_block_layout : enable

#include <lighting/brdf.glsl>
#include <includes/math.glsl>
#include "includes/buffers.glsl"

#ifdef HARDWARE_SUPPORTS_RAY_TRACING
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_ray_query : enable

layout(set = 6, binding = 0) uniform accelerationStructureEXT topLevelAS;
layout(set = 6, binding = 1) uniform texture2D noiseTexture;

layout(set = 6, binding = 2, scalar) readonly buffer Geometries {
    Geometry geometries[];
};

layout(set = 6, binding = 3, scalar) readonly buffer RTInstances {
    Instance instances[];
};
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

vec3 computeDirectLightingFromLights(inout RandomSampler rng, inout float lightPDF, inout PbrInputs pbr, vec3 worldPos, float maxDistance) {
    vec3 lightContribution = vec3(0.0);

    if(activeLights.count <= 0) {
        lightPDF = 1.0;
        return vec3(0.0);
    }
    vec3 normal = pbr.N;

    // Light Sampling, will require denoising of result
    //#define LIS // TODO: reenable
#ifdef LIS
    float pdfInv = activeLights.count;
    uint li = min(activeLights.count - 1, uint(floor(sampleNoise(rng) * activeLights.count)));
    //lightPDF = 1.0 / pdfInv;
    lightPDF = 1.0;
    do {
#else
    lightPDF = 1.0;

    [[dont_unroll]] for (uint li = 0; li < activeLights.count; li++) {
#endif
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

            pbr.L = normalize(L);
            pbr.H = normalize(pbr.L+pbr.V);

            float NdotL = dot(pbr.N, pbr.L);
            if(NdotL <= 0) {
                continue;
            }

            pbr.NdotH = dot(pbr.N, pbr.H);
            pbr.NdotL = NdotL;
            pbr.HdotL = dot(pbr.H, pbr.L);
            pbr.HdotV = dot(pbr.H, pbr.V);

            vec3 brdf = glTF_BRDF_WithoutImportanceSampling(pbr);
            if(isnan(brdf.x))
                return vec3(10,0,0);

            lightContribution += brdf * enabledF * visibility * singleLightContribution;
        }
        #undef light

#ifdef LIS
    } while(false);
#else
    }
#endif

    return lightContribution;
}
#endif