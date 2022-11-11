#include "includes/gbuffer.glsl"
#include "includes/lights.glsl"
#include "includes/debugparams.glsl"

#ifdef HARDWARE_SUPPORTS_RAY_TRACING
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_ray_query : enable

#extension GL_EXT_buffer_reference: enable
#include "includes/buffers.glsl"
#endif
#include "includes/noise/noise4D.glsl"


#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : enable

layout(push_constant) uniform PushConstant {
    uint frameCount;
    uint frameWidth;
    uint frameHeight;
} push;

layout(set = 0, binding = 0) uniform texture2D albedo;
layout(set = 0, binding = 1) uniform texture2D depth;
layout(set = 0, binding = 2) uniform texture2D viewPos;
layout(set = 0, binding = 3) uniform texture2D viewNormals;
layout(set = 0, binding = 4) uniform usampler2D intPropertiesInput;
layout(set = 0, binding = 5) uniform texture2D rayTracedLighting;
layout(set = 0, binding = 6) uniform texture2D transparent;
layout(set = 0, binding = 7) uniform texture2D roughnessMetallicValues;
layout(set = 0, binding = 8) uniform texture2D emissiveValues;
layout(set = 0, binding = 9) uniform texture2D skyboxTexture;
layout(set = 0, binding = 10) uniform sampler linearSampler;
layout(set = 0, binding = 11) uniform sampler nearestSampler;

#ifdef HARDWARE_SUPPORTS_RAY_TRACING
layout(set = 0, binding = 12) uniform accelerationStructureEXT topLevelAS;
layout(set = 0, binding = 13) uniform texture2D noiseTexture;

layout(set = 0, binding = 14) buffer Geometries {
    Geometry g[];
};

layout(set = 0, binding = 15) buffer RTInstances {
    Instance i[];
};
#endif

layout(set = 1, binding = 0) uniform CameraBufferObject {
    mat4 projection;
    mat4 view;
    mat4 inverseView;
    mat4 inverseProjection;
} cbo;

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 outColor;

LIGHT_SET(2)
DEBUG_OPTIONS_SET(3)

float rand(vec2 n) {
    return fract(sin(dot(n, vec2(12.9898, 4.1414))) * 43758.5453);
}

float sampleNoise(vec2 noiseUV) {
    const vec2 screenSize = vec2(push.frameWidth, push.frameHeight);
    float x = snoise(vec4(noiseUV * screenSize + rand(noiseUV), push.frameCount, 1.0));

    return x;
}

float computePointLight(vec3 worldPos, vec3 normal, Light light) {
    vec3 lightPosition = light.position;
    vec3 point2light = lightPosition-worldPos;

    float distance = length(point2light);
    float attenutation = light.constantAttenuation + light.linearAttenuation * distance + light.quadraticAttenuation * distance * distance;

    float inclinaisonFactor = abs(dot(normal, point2light / distance));

    return max(0, 1.0f / attenutation) * inclinaisonFactor;
}

float computeDirectionalLight(vec3 worldPos, vec3 normal, Light light) {
    vec3 lightDirection = normalize(light.direction);
    return max(0, dot(lightDirection, normal));
}

float computeSpotLight(vec3 worldPos, vec3 normal, Light light) {
    float cutoffCosAngle = light.cutoffCosAngle;
    float outerCosCutoffAngle = light.outerCosCutoffAngle;

    vec3 lightPosition = light.position;
    vec3 point2light = normalize(lightPosition-worldPos);
    vec3 lightDirection = -normalize(light.direction);
    float intensity = dot(point2light, lightDirection);
    float cutoffRange = outerCosCutoffAngle - cutoffCosAngle;
    return clamp((intensity - outerCosCutoffAngle) / cutoffRange, 0, 1);
}

/**
* Computes the direct contribution of the given light
*/
vec3 computeLightContribution(uint lightIndex, vec3 worldPos, vec3 normal) {
    #define light lights.l[lightIndex]
    float enabledF = float(light.enabled);

    float lightFactor = 0.0f;
    switch(light.type) {
        case POINT_LIGHT_TYPE:
            lightFactor = computePointLight(worldPos, normal, light);
        break;

        case DIRECTIONAL_LIGHT_TYPE:
            lightFactor = computeDirectionalLight(worldPos, normal, light);
        break;

        case SPOT_LIGHT_TYPE:
            lightFactor = computeSpotLight(worldPos, normal, light);
        break;
    }

    return enabledF * light.color * lightFactor * light.intensity;
    #undef light
}

#ifdef HARDWARE_SUPPORTS_RAY_TRACING
vec3 Li(vec2 noiseUVStart, vec3 worldPos, vec3 direction, float maxDistance) {
    #define light lights.l[i]
    vec3 lightContribution = vec3(0.0);

    float pdfInv = 1.0;
    #if 0
    for(uint i = 0; i < lights.count; i++)
    #else
    pdfInv = lights.count;
    uint i = uint(floor(sampleNoise(noiseUVStart) * lights.count));
    #endif
    {
        float shadowPercent = 0.0f;
        float enabledF = float(light.enabled);

        if(light.enabled)
        {
            vec3 L = vec3(0, 0, 1);
            switch (light.type) {
                case POINT_LIGHT_TYPE:
                L = light.position - worldPos;
                break;

                case DIRECTIONAL_LIGHT_TYPE:
                L = light.direction;
                break;

                case SPOT_LIGHT_TYPE:
                L = light.position - worldPos;
                break;
            }

            // from https://github.com/nvpro-samples/vk_raytracing_tutorial_KHR/tree/master/ray_tracing_rayquery
            float lightDistance = 5000.0f;// TODO: compute this value properly

            if(light.type != DIRECTIONAL_LIGHT_TYPE) {
                lightDistance = min(lightDistance, length(L));
            }

            // Ray Query for shadow
            vec3  origin    = worldPos;
            vec3  rayDirection = normalize(L);// vector to light
            float tMin      = 0.001f;
            float tMax      = enabledF * lightDistance;

            // Initializes a ray query object but does not start traversal
            rayQueryEXT rayQuery;
            rayQueryInitializeEXT(rayQuery, topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsCullBackFacingTrianglesEXT , 0xFF, origin, tMin, rayDirection, tMax);

            // Start traversal: return false if traversal is complete
            while(rayQueryProceedEXT(rayQuery)) {}

            // Returns type of committed (true) intersection
            shadowPercent = float(rayQueryGetIntersectionTypeEXT(rayQuery, true) != gl_RayQueryCommittedIntersectionNoneEXT);

            const vec3 normal = direction; // TODO
            lightContribution += enabledF * (1.0f - shadowPercent) * computeLightContribution(i, origin, normal);
        }
    }
    return lightContribution * pdfInv;
    #undef light
}
#endif


vec3 dBSDF(vec3 worldPos, vec3 emitDirection, vec3 incidentDirection) {
    // TODO: depend on material
    return vec3(1.0);
}

vec3 calculateLighting(vec2 noiseUVStart, vec3 worldPos, vec3 emissive, vec3 normal, bool raytracing) {
    vec3 finalColor = vec3(0.0);

    const vec3 cameraForward = (cbo.inverseView * vec4(0, 0, 1, 0)).xyz;

#ifdef HARDWARE_SUPPORTS_RAY_TRACING
    if (!raytracing)
    {
#endif
        vec3 lightContribution = emissive;
        for (uint i = 0; i < lights.count; i++) {
            lightContribution += computeLightContribution(i, worldPos, normal);
        }
        return lightContribution;
#ifdef HARDWARE_SUPPORTS_RAY_TRACING
    }
    else
    {
        vec3 lightContribution = emissive;
        const uint SAMPLE_COUNT = 10; // TODO: more samples

        vec3 wo = -cameraForward;
        for(uint i = 0; i < SAMPLE_COUNT; i++) {

            vec3 wi = normal; // TODO: depend on i
            lightContribution += dBSDF(worldPos, wo, wi) * Li(noiseUVStart + vec2(i), worldPos, normal, 5000.0f) * dot(wi, normal); // TODO: cos() multiplied twice right now due to Li
        }

        return lightContribution / SAMPLE_COUNT;
    }
#endif
}

void main() {
    vec4 outColorWorld;

    vec4 fragmentColor = texture(sampler2D(albedo, linearSampler), uv);
    vec3 viewPos = texture(sampler2D(viewPos, linearSampler), uv).xyz;
    vec3 worldPos = (cbo.inverseView * vec4(viewPos, 1.0)).xyz;
    vec3 viewNormal = texture(sampler2D(viewNormals, linearSampler), uv).xyz;
    vec3 normal = normalize((cbo.inverseView * vec4(viewNormal, 0.0)).xyz);
    vec3 skyboxRGB = texture(sampler2D(skyboxTexture, linearSampler), uv).rgb;
    vec2 roughnessMetallic = texture(sampler2D(roughnessMetallicValues, linearSampler), uv).rg;
    vec3 emissive = texture(sampler2D(emissiveValues, linearSampler), uv).rgb;

    uint intProperties = texture(intPropertiesInput, uv).r;
    float currDepth = texture(sampler2D(depth, linearSampler), uv).r;

    if(debug.gBufferType == DEBUG_GBUFFER_ALBEDO) {
        outColor = fragmentColor;
        return;
    } else if(debug.gBufferType == DEBUG_GBUFFER_DEPTH) {
        outColor = vec4(currDepth, currDepth, currDepth, 1.0);
        return;
    } else if(debug.gBufferType == DEBUG_GBUFFER_POSITION) {
        outColor = vec4(viewPos, 1.0);
        return;
    } else if(debug.gBufferType == DEBUG_GBUFFER_NORMAL) {
        outColor = vec4(viewNormal, 1.0);
        return;
    } else if(debug.gBufferType == DEBUG_GBUFFER_METALLIC_ROUGHNESS) {
        outColor = vec4(roughnessMetallic, 0.0, 1.0);
        return;
    } else if(debug.gBufferType == DEBUG_GBUFFER_EMISSIVE) {
        outColor = vec4(emissive, 1.0);
        return;
    } else if(debug.gBufferType == DEBUG_GBUFFER_RANDOMNESS) {
        outColor = vec4(sampleNoise(uv).rrr, 1.0);
        return;
    }

    if(currDepth < 1.0) {
        if((intProperties & IntPropertiesRayTracedLighting) == IntPropertiesRayTracedLighting) {
            outColorWorld = vec4(fragmentColor.rgb, fragmentColor.a);
        } else {
            outColorWorld = fragmentColor;
        }

        outColorWorld.rgb *= calculateLighting(uv, worldPos, emissive, normal, true);

        float distanceToCamera = length(viewPos);

        float fogFactor = clamp((distanceToCamera - lights.fogDistance) / lights.fogDepth, 0, 1);

        outColorWorld.rgb = mix(outColorWorld.rgb, lights.fogColor, fogFactor);
    } else {
        outColorWorld = vec4(skyboxRGB, 1.0);
    }

    vec4 transparentColor = texture(sampler2D(transparent, linearSampler), uv);

    vec3 blended = outColorWorld.rgb * (1.0 - transparentColor.a) + transparentColor.rgb * transparentColor.a;

    outColor = vec4(blended, 1.0);
//    outColor = vec4(currDepth.rrr, 1.0);
}