#include "includes/gbuffer.glsl"
#include "includes/lights.glsl"

#ifdef HARDWARE_SUPPORTS_RAY_TRACING
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_ray_query : enable
#endif

#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : enable

layout(set = 0, binding = 0) uniform texture2D albedo;
layout(set = 0, binding = 1) uniform texture2D depth;
layout(set = 0, binding = 2) uniform texture2D viewPos;
layout(set = 0, binding = 3) uniform texture2D viewNormals;
layout(set = 0, binding = 4) uniform usampler2D intPropertiesInput;
layout(set = 0, binding = 5) uniform texture2D rayTracedLighting;
layout(set = 0, binding = 6) uniform texture2D transparent;
layout(set = 0, binding = 7) uniform texture2D skyboxTexture;
layout(set = 0, binding = 8) uniform sampler linearSampler;
layout(set = 0, binding = 9) uniform sampler nearestSampler;

#ifdef HARDWARE_SUPPORTS_RAY_TRACING
layout(binding = 10, set = 0) uniform accelerationStructureEXT topLevelAS;
#endif

/*
layout(set = 0, binding = 11) uniform Debug {
    #include "debugparams.glsl"
} debug;*/


layout(set = 2, binding = 0) uniform CameraBufferObject {
    mat4 projection;
    mat4 view;
    mat4 inverseView;
    mat4 inverseProjection;
} cbo;

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 outColor;

LIGHT_SET(3)

float computePointLight(vec3 worldPos, vec3 normal, Light light) {
    vec3 lightPosition = light.position;
    vec3 point2light = lightPosition-worldPos;

    float distance = length(point2light);
    float attenutation = light.constantAttenuation + light.linearAttenuation * distance + light.quadraticAttenuation * distance * distance;

    return max(0, 1.0f / attenutation);
}

float computeDirectionalLight(vec3 worldPos, vec3 normal, Light light) {
    vec3 lightDirection = -normalize(light.direction);
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

vec3 calculateLighting(vec3 worldPos, vec3 normal, bool computeShadows) {
    vec3 finalColor = vec3(0.0);

    vec3 lightContribution = vec3(0.0);

    for(uint i = 0; i < lights.count; i++) {
        #define light lights.l[i]

        if (!light.enabled)
        continue;

        // is this point in shadow?
        float shadowPercent = 0.0f;

        if (!computeShadows) {
            shadowPercent = 0.0f;
        } else {
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

            #ifdef HARDWARE_SUPPORTS_RAY_TRACING
            // from https://github.com/nvpro-samples/vk_raytracing_tutorial_KHR/tree/master/ray_tracing_rayquery
            float lightDistance = 5000.0f;// TODO: compute this value properly

            if(light.type != DIRECTIONAL_LIGHT_TYPE) {
                lightDistance = min(lightDistance, length(L));
            }

            // Ray Query for shadow
            vec3  origin    = worldPos;
            vec3  direction = normalize(L);// vector to light
            float tMin      = 0.001f;
            float tMax      = lightDistance;

            // Initializes a ray query object but does not start traversal
            rayQueryEXT rayQuery;
            rayQueryInitializeEXT(rayQuery, topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsCullBackFacingTrianglesEXT , 0xFF, origin, tMin, direction, tMax);

            // Start traversal: return false if traversal is complete
            while(rayQueryProceedEXT(rayQuery)) {}

            // Returns type of committed (true) intersection
            if(rayQueryGetIntersectionTypeEXT(rayQuery, true) != gl_RayQueryCommittedIntersectionNoneEXT) {
                // Got an intersection == Shadow
                shadowPercent = 1.0f;
            }
            #endif
        }

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
        lightContribution += light.color * lightFactor * light.intensity * (1.0f - shadowPercent);
    }

    lightContribution = min(vec3(1.0)-lights.ambientColor, lightContribution);

    return lightContribution + lights.ambientColor;
}

void main() {
    vec4 outColorWorld;

    vec4 fragmentColor = texture(sampler2D(albedo, linearSampler), uv);
    vec3 worldPos = (cbo.inverseView * vec4(texture(sampler2D(viewPos, linearSampler), uv).xyz, 1.0)).xyz;
    vec3 normal = normalize((cbo.inverseView * vec4(texture(sampler2D(viewNormals, linearSampler), uv).xyz, 0.0)).xyz);
    vec3 skyboxRGB = texture(sampler2D(skyboxTexture, linearSampler), uv).rgb;

    uint intProperties = texture(intPropertiesInput, uv).r;
    float currDepth = texture(sampler2D(depth, linearSampler), uv).r;
    if(currDepth < 1.0) {
        if((intProperties & IntPropertiesRayTracedLighting) == IntPropertiesRayTracedLighting) {
            outColorWorld = vec4(fragmentColor.rgb, fragmentColor.a);
        } else {
            outColorWorld = fragmentColor;
        }
        outColorWorld.rgb *= calculateLighting(worldPos, normal, true);
    } else {
        outColorWorld = vec4(skyboxRGB, 1.0);
    }

    vec4 transparentColor = texture(sampler2D(transparent, linearSampler), uv);

    vec3 blended = outColorWorld.rgb * (1.0 - transparentColor.a) + transparentColor.rgb * transparentColor.a;

    outColor = vec4(blended, 1.0);
//    outColor = vec4(currDepth.rrr, 1.0);
}