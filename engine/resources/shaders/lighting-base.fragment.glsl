#include "includes/gbuffer.glsl"
#include "includes/lights.glsl"
#include "includes/debugparams.glsl"
#include "includes/materials.glsl"
#include "includes/math.glsl"

#ifdef HARDWARE_SUPPORTS_RAY_TRACING
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_ray_query : enable

#extension GL_EXT_buffer_reference: enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable
#extension GL_EXT_buffer_reference2 : enable
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
layout(set = 0, binding = 7) uniform texture2D metallicRoughnessValues;
layout(set = 0, binding = 8) uniform texture2D emissiveValues;
layout(set = 0, binding = 9) uniform texture2D skyboxTexture;
layout(set = 0, binding = 10) uniform texture2D viewTangents;


#ifdef HARDWARE_SUPPORTS_RAY_TRACING
layout(set = 0, binding = 12) uniform accelerationStructureEXT topLevelAS;
layout(set = 0, binding = 13) uniform texture2D noiseTexture;

layout(set = 0, binding = 14, scalar) buffer Geometries {
    Geometry geometries[];
};

layout(set = 0, binding = 15, scalar) buffer RTInstances {
    Instance instances[];
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
MATERIAL_SYSTEM_SET(4)

struct SurfaceIntersection {
    vec3 position;
    uint materialIndex;
    vec3 surfaceNormal;
    vec3 surfaceTangent;
    vec2 uv;
    bool hasIntersection;
};

struct RandomSampler {
    vec2 pixelUV;
    uint seed;
};

float rand(vec2 n) {
    return fract(sin(dot(n, vec2(12.9898, 4.1414))) * 43758.5453);
}

uint wang_hash(uint seed) {
    seed = (seed ^ 61u) ^ (seed >> 16);
    seed *= 9;
    seed = seed ^ (seed >> 4);
    seed *= 0x27d4eb2d;
    seed = seed ^ (seed >> 15);
    return seed;
}

float sampleNoise(inout RandomSampler rng) {
    const vec2 screenSize = vec2(push.frameWidth, push.frameHeight);

    const vec2 pixelPos = rng.pixelUV * screenSize;
    const uint pixelIndex = uint(pixelPos.x + pixelPos.y * push.frameWidth);
    rng.seed = wang_hash(push.frameCount * pixelIndex + rng.seed);
    return rng.seed * (1.0 / 4294967296.0);
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
bool traceShadowRay(vec3 startPos, vec3 direction, float maxDistance) {
    // Ray Query for shadow
    float tMin      = 0.001f;

    // Initializes a ray query object but does not start traversal
    rayQueryEXT rayQuery;
    rayQueryInitializeEXT(rayQuery, topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, startPos, tMin, direction, maxDistance);

    // Start traversal: return false if traversal is complete
    while(rayQueryProceedEXT(rayQuery)) {}

    // Returns type of committed (true) intersection
    return rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionNoneEXT;
}

SurfaceIntersection traceRay(vec3 startPos, vec3 direction, float maxDistance) {
    SurfaceIntersection r;
    r.hasIntersection = false;
    // Ray Query for shadow
    float tMin      = 0.00001f;

    // Initializes a ray query object but does not start traversal
    rayQueryEXT rayQuery;
    rayQueryInitializeEXT(rayQuery, topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, startPos, tMin, direction, maxDistance);

    // Start traversal: return false if traversal is complete
    while(rayQueryProceedEXT(rayQuery)) {}

    // Returns type of committed (true) intersection
    if(rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionNoneEXT) {
        return r;
    }

    int instanceID = rayQueryGetIntersectionInstanceIdEXT(rayQuery, true);
    if(instanceID < 0) {
        return r;
    }
    int localGeometryID = rayQueryGetIntersectionGeometryIndexEXT(rayQuery, true);
    if(localGeometryID < 0) {
        return r;
    }

    #define instance (instances[nonuniformEXT(instanceID)])

    // TODO: bounds checks?
    uint geometryID = instance.firstGeometryIndex + localGeometryID;
    int triangleID = rayQueryGetIntersectionPrimitiveIndexEXT(rayQuery, true);
    if(triangleID < 0) {
        return r;
    }
    vec2 barycentrics = rayQueryGetIntersectionBarycentricsEXT(rayQuery, true);

    #define geometry (geometries[nonuniformEXT(geometryID)])

    Vertex P2 = (geometry.vertexBuffer.v[nonuniformEXT(geometry.indexBuffer.i[nonuniformEXT(triangleID*3+0)])]);
    Vertex P0 = (geometry.vertexBuffer.v[nonuniformEXT(geometry.indexBuffer.i[nonuniformEXT(triangleID*3+1)])]);
    Vertex P1 = (geometry.vertexBuffer.v[nonuniformEXT(geometry.indexBuffer.i[nonuniformEXT(triangleID*3+2)])]);

    vec2 uvPos = P0.uv * barycentrics.x + P1.uv * barycentrics.y + P2.uv * (1.0 - barycentrics.x - barycentrics.y);
    vec3 surfaceNormal = P0.normal * barycentrics.x + P1.normal * barycentrics.y + P2.normal * (1.0 - barycentrics.x - barycentrics.y);
    vec3 surfaceTangent = P0.tangent * barycentrics.x + P1.tangent * barycentrics.y + P2.tangent * (1.0 - barycentrics.x - barycentrics.y);

    mat3 modelRotation = mat3(rayQueryGetIntersectionObjectToWorldEXT(rayQuery, true));
    r.hasIntersection = true;
    r.materialIndex = geometry.materialIndex;
    r.uv = uvPos;
    r.surfaceNormal = modelRotation * surfaceNormal;
    r.surfaceTangent = modelRotation * surfaceTangent;
    r.position = rayQueryGetIntersectionTEXT(rayQuery, true) * direction + startPos;
    return r;
}

vec3 computeDirectLighting(inout RandomSampler rng, vec3 worldPos, vec3 direction, vec3 normal, float maxDistance) {
    #define light lights.l[i]
    vec3 lightContribution = vec3(0.0);

    float pdfInv = 1.0;
    #if 0
    for(uint i = 0; i < lights.count; i++)
    #else
    pdfInv = lights.count;
    uint i = uint(floor(sampleNoise(rng) * lights.count));
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
            float lightDistance = maxDistance;

            if(light.type != DIRECTIONAL_LIGHT_TYPE) {
                lightDistance = min(lightDistance, length(L));
            }

            float visibility = float(traceShadowRay(worldPos, normalize(L), lightDistance * enabledF));
            lightContribution += enabledF * visibility * computeLightContribution(i, worldPos, normal) /* cos term already in computeLightContribution */;
        }
    }
    return lightContribution * pdfInv;
    #undef light
}
#endif

// ============= TANGENT SPACE ONLY =============
float computePDF(vec3 wo, vec3 wi) {
    // TODO: GGX instead of Lambertian
    if(wo.z * wi.z < 0) {
        return 0.0f;
    }

    return abs(wi.z) * M_INV_PI;
}

vec2 concentricSampleDisk(inout RandomSampler rng) {
    vec2 u = vec2(sampleNoise(rng), sampleNoise(rng)) * 2.0 - 1.0;
    if(u.x == 0 && u.y == 0)
        return vec2(0.0);

    float theta = 0.0f;
    float r = 0.0f;

    if(abs(u.x) > abs(u.y)) {
        r = u.x;
        theta = M_PI_OVER_4 * (u.y / u.x);
    } else {
        r = u.y;
        theta = M_PI_OVER_2 - M_PI_OVER_4 * (u.x / u.y);
    }
    return r * vec2(cos(theta), sin(theta));
}

vec3 cosineSampleHemisphere(inout RandomSampler rng) {
    vec2 d = concentricSampleDisk(rng);
    float z = sqrt(max(0, 1 - dot(d, d)));
    return vec3(d.x, d.y, z);
}

vec3 sampleDirection(inout RandomSampler rng, vec3 emitDirection, inout vec3 incidentDirection, inout float pdf) {
    // TODO: GGX instead of Lambertian

    incidentDirection = cosineSampleHemisphere(rng);

    float woDotN = emitDirection.z;
    if(woDotN < 0) incidentDirection.z *= -1.0;

    pdf = computePDF(emitDirection, incidentDirection);

    return vec3(M_INV_PI);
}

// ============= END OF TANGENT SPACE ONLY =============

vec3 calculateLighting(inout RandomSampler rng, vec3 worldPos, vec3 emissive, vec3 normal, vec3 tangent, vec2 metallicRoughness, bool raytracing) {
    vec3 finalColor = vec3(0.0);

    const vec3 cameraForward = (cbo.inverseView * vec4(0, 0, 1, 0)).xyz;
    const vec3 cameraPos = (cbo.inverseView * vec4(0, 0, 0, 1)).xyz;

#ifdef HARDWARE_SUPPORTS_RAY_TRACING
    if (!raytracing)
    {
#endif
        vec3 lightContribution = emissive + lights.ambientColor;
        for (uint i = 0; i < lights.count; i++) {
            lightContribution += computeLightContribution(i, worldPos, normal);
        }
        return lightContribution;
#ifdef HARDWARE_SUPPORTS_RAY_TRACING
    }
    else
    {
        vec3 lightContribution = lights.ambientColor;

        vec3 incomingRay = normalize(worldPos - cameraPos);
        const float roughness = metallicRoughness.y;
        const float metallic = metallicRoughness.x;
        const float MAX_LIGHT_DISTANCE = 5000.0f; /* TODO: specialization constant? compute properly?*/
        const uint MAX_BOUNCES = 3; /* TODO: specialization constant?*/

        vec3 beta = vec3(1.0f);
        int depth = 0;

        vec3 pathContribution = vec3(0.0f);
        bool lastIsSpecular = false;
        for(; depth < MAX_BOUNCES; depth++) {
            const float u = sampleNoise(rng);
            SurfaceIntersection intersection;
            intersection.hasIntersection = false;


            // direct lighting
            pathContribution += (computeDirectLighting(rng, worldPos, incomingRay, normal, MAX_LIGHT_DISTANCE)) * beta;

            if(u <= metallic) {
                // sample specular reflection
                vec3 reflectedRay = reflect(incomingRay, normal);
                intersection = traceRay(worldPos, reflectedRay, MAX_LIGHT_DISTANCE);
                incomingRay = reflectedRay;
            } else {
                // TODO: use GGX instead of Lambertian

                vec3 T = tangent;
                vec3 N = normal;
                T = normalize(T - dot(T, N) * N);
                vec3 B = cross(T, N);

                mat3 tbn = mat3(T, B, N);
                mat3 invTBN = inverse(tbn);

                vec3 direction;
                float pdf;
                vec3 f = sampleDirection(rng, invTBN * incomingRay, direction, pdf);

                if(pdf == 0.0)
                    break;

                vec3 worldSpaceDirection = tbn * -direction;
                beta *= f * abs(dot(worldSpaceDirection, N)) / pdf;

                intersection = traceRay(worldPos, worldSpaceDirection, MAX_LIGHT_DISTANCE);
                incomingRay = worldSpaceDirection;
            }

            if(intersection.hasIntersection) {
                #define _sample(TYPE) texture(sampler2D(textures[materials[intersection.materialIndex].TYPE], linearSampler), intersection.uv)
                beta *= _sample(albedo).rgb;

                worldPos = intersection.position;
                emissive = _sample(emissive).rgb;

                vec3 T = intersection.surfaceTangent;
                vec3 N = intersection.surfaceNormal;
                T = normalize(T - dot(T, N) * N);
                vec3 B = cross(T, N);

                mat3 tbn = mat3(T, B, N);
                normal = tbn * _sample(normalMap).rgb;
                tangent = tbn * vec3(1, 0, 0);
                metallicRoughness = _sample(metallicRoughness).rg;
            } else {
                vec3 skyboxColor = vec3(0,0,0); // TODO
                lightContribution += skyboxColor * beta;
                break;
            }
        }

        return lightContribution + pathContribution;
    }
#endif
}

void main() {
    vec4 outColorWorld;

    vec4 fragmentColor = texture(sampler2D(albedo, linearSampler), uv);
    vec3 viewPos = texture(sampler2D(viewPos, linearSampler), uv).xyz;
    vec3 worldPos = (cbo.inverseView * vec4(viewPos, 1.0)).xyz;
    vec3 viewNormal = texture(sampler2D(viewNormals, linearSampler), uv).xyz;
    vec3 viewTangent = texture(sampler2D(viewTangents, linearSampler), uv).xyz;
    vec3 normal = normalize((cbo.inverseView * vec4(viewNormal, 0.0)).xyz);
    vec3 tangent = normalize((cbo.inverseView * vec4(viewTangent, 0.0)).xyz);
    vec3 skyboxRGB = texture(sampler2D(skyboxTexture, linearSampler), uv).rgb;
    vec2 metallicRoughness = texture(sampler2D(metallicRoughnessValues, linearSampler), uv).rg;
    vec3 emissive = texture(sampler2D(emissiveValues, linearSampler), uv).rgb;

    uint intProperties = uint(texture(intPropertiesInput, uv).r);
    float currDepth = texture(sampler2D(depth, linearSampler), uv).r;

    RandomSampler rng;
    rng.seed = 0;
    rng.pixelUV = uv;

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
        outColor = vec4(metallicRoughness, 0.0, 1.0);
        return;
    } else if(debug.gBufferType == DEBUG_GBUFFER_EMISSIVE) {
        outColor = vec4(emissive, 1.0);
        return;
    } else if(debug.gBufferType == DEBUG_GBUFFER_RANDOMNESS) {
        outColor = vec4(sampleNoise(rng).rrr, 1.0);
        return;
    } else if(debug.gBufferType == DEBUG_GBUFFER_TANGENT) {
        outColor = vec4(viewTangent, 1.0);
        return;
    }

    float distanceToCamera;
    if(currDepth < 1.0) {
        if((intProperties & IntPropertiesRayTracedLighting) == IntPropertiesRayTracedLighting) {
            outColorWorld = vec4(fragmentColor.rgb, fragmentColor.a);
        } else {
            outColorWorld = fragmentColor;
        }

#ifdef HARDWARE_SUPPORTS_RAY_TRACING
        const int SAMPLE_COUNT = 2; // TODO: configurable sample count?
        const float INV_SAMPLE_COUNT = 1.0f / SAMPLE_COUNT; // TODO: configurable sample count?
        vec3 l = vec3(0.0);
        for(int i = 0; i < SAMPLE_COUNT; i++) {
            l += calculateLighting(rng, worldPos, emissive, normal, tangent, metallicRoughness, true);
        }
        outColorWorld.rgb *= l * INV_SAMPLE_COUNT;
#else
        outColorWorld.rgb *= calculateLighting(rng, worldPos, emissive, normal, tangent, metallicRoughness, true);
#endif

        distanceToCamera = length(viewPos);
    } else {
        outColorWorld = vec4(skyboxRGB, 1.0);
        distanceToCamera = 1.0f/0.0f;
    }

    float fogFactor = clamp((distanceToCamera - lights.fogDistance) / lights.fogDepth, 0, 1);
    outColorWorld.rgb = mix(outColorWorld.rgb, lights.fogColor, fogFactor);

    vec4 transparentColor = texture(sampler2D(transparent, linearSampler), uv);

    vec3 blended = outColorWorld.rgb * (1.0 - transparentColor.a) + transparentColor.rgb * transparentColor.a;

    outColor = vec4(blended, 1.0);
}