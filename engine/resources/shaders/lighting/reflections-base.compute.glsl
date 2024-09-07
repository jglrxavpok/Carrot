#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int32 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable

#include "includes/lights.glsl"
#include "includes/materials.glsl"
#include <includes/gbuffer.glsl>

#extension GL_EXT_control_flow_attributes: enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

const uint LOCAL_SIZE_X = 32;
const uint LOCAL_SIZE_Y = 32;
layout (local_size_x = LOCAL_SIZE_X) in;
layout (local_size_y = LOCAL_SIZE_Y) in;

layout(push_constant) uniform PushConstant {
    uint frameCount;
    uint frameWidth;
    uint frameHeight;

#ifdef HARDWARE_SUPPORTS_RAY_TRACING
    bool hasTLAS; // handle special cases where no raytraceable geometry is present in the scene
#endif
} push;

#include "includes/gbuffer_input.glsl"
#include <includes/camera.glsl>

DEFINE_GBUFFER_INPUTS(0)
#include "includes/gbuffer_unpack.glsl"

DEFINE_CAMERA_SET(1)
LIGHT_SET(2)
MATERIAL_SYSTEM_SET(4)

#include "includes/rng.glsl"

layout(rgba32f, set = 5, binding = 0) uniform writeonly image2D outDirectLightingImage;

// needs to be included after LIGHT_SET macro & RT data
#include <lighting/base.common.glsl>

struct SurfaceIntersection {
    vec3 position;
    uint materialIndex;
    vec3 surfaceNormal;
    vec3 surfaceTangent;
    vec3 surfaceColor;
    vec2 uv;
    float bitangentSign;
    bool hasIntersection;
};

#ifdef HARDWARE_SUPPORTS_RAY_TRACING
SurfaceIntersection intersection;

void traceRayWithSurfaceInfo(inout SurfaceIntersection r, vec3 startPos, vec3 direction, float maxDistance) {
    r.hasIntersection = false;
    if(!push.hasTLAS) {
        return;
    }

    float tMin      = 0.00001f;

    rayQueryInitializeEXT(rayQuery, topLevelAS, gl_RayFlagsSkipClosestHitShaderEXT | gl_RayFlagsOpaqueEXT, 0xFF, startPos, tMin, direction, maxDistance);

    while(rayQueryProceedEXT(rayQuery)){}

    // Returns type of committed (true) intersection
    if(rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionNoneEXT) {
        return;
    }

    int instanceID = rayQueryGetIntersectionInstanceIdEXT(rayQuery, true);
    if(instanceID < 0) {
        return;
    }
    int localGeometryID = rayQueryGetIntersectionGeometryIndexEXT(rayQuery, true);
    if(localGeometryID < 0) {
        return;
    }

    Instance instance = instances[nonuniformEXT(instanceID)];
    r.surfaceColor = instance.instanceColor.rgb;

    // TODO: bounds checks?
    uint geometryID = instance.firstGeometryIndex + localGeometryID;
    int triangleID = rayQueryGetIntersectionPrimitiveIndexEXT(rayQuery, true);
    if(triangleID < 0) {
        return;
    }
    vec2 barycentrics = rayQueryGetIntersectionBarycentricsEXT(rayQuery, true);


    Geometry geometry = (geometries[nonuniformEXT(geometryID)]);

    vec3 surfaceNormal;
    vec3 surfaceTangent;
    vec3 color;
    const uint geometryFormat = geometry.geometryFormat;
    switch(geometryFormat) {
        case GEOMETRY_FORMAT_DEFAULT: {
            #define index2 (nonuniformEXT(geometry.indexBuffer.i[nonuniformEXT(triangleID*3+0)]))
            #define index0 (nonuniformEXT(geometry.indexBuffer.i[nonuniformEXT(triangleID*3+1)]))
            #define index1 (nonuniformEXT(geometry.indexBuffer.i[nonuniformEXT(triangleID*3+2)]))

            #define P2 (geometry.vertexBuffer.v[index2])
            #define P0 (geometry.vertexBuffer.v[index0])
            #define P1 (geometry.vertexBuffer.v[index1])

            r.uv = P0.uv * barycentrics.x + P1.uv * barycentrics.y + P2.uv * (1.0 - barycentrics.x - barycentrics.y);
            surfaceNormal = normalize(P0.normal * barycentrics.x + P1.normal * barycentrics.y + P2.normal * (1.0 - barycentrics.x - barycentrics.y));

            vec4 tangentWithBitangentSign = (P0.tangent * barycentrics.x + P1.tangent * barycentrics.y + P2.tangent * (1.0 - barycentrics.x - barycentrics.y));
            surfaceTangent = normalize(tangentWithBitangentSign.xyz);
            r.bitangentSign = tangentWithBitangentSign.w;
            r.surfaceColor *= P0.color * barycentrics.x + P1.color * barycentrics.y + P2.color * (1.0 - barycentrics.x - barycentrics.y);
            #undef P0
            #undef P1
            #undef P2

            #undef index2
            #undef index0
            #undef index1
        } break;

        case GEOMETRY_FORMAT_PACKED: {
            IndexBuffer16 idxBuffer = IndexBuffer16(uint64_t(geometry.indexBuffer));
            #define index2 (nonuniformEXT(idxBuffer.i[nonuniformEXT(triangleID*3+0)]))
            #define index0 (nonuniformEXT(idxBuffer.i[nonuniformEXT(triangleID*3+0)]))
            #define index1 (nonuniformEXT(idxBuffer.i[nonuniformEXT(triangleID*3+2)]))

            PackedVertexBuffer vtxBuffer = PackedVertexBuffer(uint64_t(geometry.vertexBuffer));
            #define P2 (vtxBuffer.v[index2])
            #define P0 (vtxBuffer.v[index0])
            #define P1 (vtxBuffer.v[index1])

            r.uv = P0.uv * barycentrics.x + P1.uv * barycentrics.y + P2.uv * (1.0 - barycentrics.x - barycentrics.y);
            surfaceNormal = normalize(P0.normal * barycentrics.x + P1.normal * barycentrics.y + P2.normal * (1.0 - barycentrics.x - barycentrics.y));
            vec4 tangentWithBitangentSign = (P0.tangent * barycentrics.x + P1.tangent * barycentrics.y + P2.tangent * (1.0 - barycentrics.x - barycentrics.y));
            surfaceTangent = normalize(tangentWithBitangentSign.xyz);
            r.bitangentSign = tangentWithBitangentSign.w;
            r.surfaceColor *= P0.color * barycentrics.x + P1.color * barycentrics.y + P2.color * (1.0 - barycentrics.x - barycentrics.y);
            #undef P0
            #undef P1
            #undef P2

            #undef index2
            #undef index0
            #undef index1
        } break;

        default: {
            // maybe crashing would be better? not sure
            r.hasIntersection = false;
            return;
        } break;
    }

    mat3 normalMatrix = mat3(rayQueryGetIntersectionObjectToWorldEXT(rayQuery, true));
    normalMatrix = inverse(normalMatrix);
    normalMatrix = transpose(normalMatrix);
    r.hasIntersection = true;
    r.materialIndex = geometry.materialIndex;
    r.surfaceNormal = normalize(normalMatrix * surfaceNormal);
    r.surfaceTangent = normalize(normalMatrix * surfaceTangent);
    r.position = rayQueryGetIntersectionTEXT(rayQuery, true) * direction + startPos;
}
#endif

float computeLambertian_PDF_NoPI(vec3 wo, vec3 wi) {
    if (wo.z * wi.z < 0) {
        return 0.0f;
    }

    return abs(wi.z);
}

float sampleLambertianF_NoPI(inout RandomSampler rng, float roughness, vec3 emitDirection, inout vec3 incidentDirection, inout float pdf) {
    incidentDirection = cosineSampleHemisphere(rng);
    float woDotN = emitDirection.z;
    if(woDotN < 0) incidentDirection.z *= -1.0;
    pdf = computeLambertian_PDF_NoPI(emitDirection, incidentDirection);

    return 1.0f;
}

vec3 calculateReflections(inout RandomSampler rng, float roughness, vec3 worldPos, vec3 normal, mat3 TBN, mat3 invTBN, bool raytracing) {
    const vec3 cameraPos = (cbo.inverseView * vec4(0, 0, 0, 1)).xyz;

    vec3 toCamera = worldPos - cameraPos;
    vec3 incomingRay = normalize(toCamera);

    vec3 perfectReflection = reflect(incomingRay, normal);
    float probabilityFactor;
    vec3 direction;
    if(roughness >= 0.01f) { // maybe a subgroup operation would be better?
        // mix between perfect reflection and perfect diffusion
        // probably not physically accurate, but dump enough for me to understand
        vec3 lambertianDirection;
        float pdf;
        float f = sampleLambertianF_NoPI(rng, roughness, invTBN * incomingRay, lambertianDirection, pdf);
        probabilityFactor = f / (pdf * roughness);
        direction = mix(perfectReflection, TBN * lambertianDirection, roughness);
    } else {
        probabilityFactor = 1.0f;
        direction = perfectReflection;
    }

    #ifdef HARDWARE_SUPPORTS_RAY_TRACING
    if (!raytracing)
    {
        #endif
        vec3 worldViewDir = direction;

        const mat3 rot = mat3(
            vec3(1.0, 0.0, 0.0),
            vec3(0.0, 0.0, -1.0),
            vec3(0.0, 1.0, 0.0)
        );
        vec3 skyboxRGB = texture(gSkybox3D, (rot) * worldViewDir).rgb;

        return skyboxRGB.rgb;
        #ifdef HARDWARE_SUPPORTS_RAY_TRACING
    }
    else
    {
        float distanceToCamera = length(toCamera);
        const float tMax = 5000.0f; /* TODO: specialization constant? compute properly?*/
        const vec3 rayOrigin = worldPos;

        intersection.hasIntersection = false;

        traceRayWithSurfaceInfo(intersection, rayOrigin, direction, tMax);
        if(intersection.hasIntersection) {
            // from "Raytraced reflections in 'Wolfenstein: Young Blood'":
            float mip = max(log2(3840.0 / push.frameWidth), 0.0);
            vec2 f;
            float viewDistance = length(worldPos - rayOrigin);
            float hitDistance = length(intersection.position - rayOrigin);
            f.x = viewDistance; // ray origin
            f.y = hitDistance; // ray length
            f = clamp(f / tMax, 0, 1);
            f = sqrt(f);
            mip += f.x * 10.0f;
            mip += f.y * 10.0f;

            #define _sample(TYPE) textureLod(sampler2D(textures[materials[intersection.materialIndex].TYPE], linearSampler), intersection.uv, floor(mip))

            vec3 albedo = _sample(albedo).rgb;
            vec3 emissive = _sample(emissive).rgb;

            // normal mapping
            vec3 mappedNormal;
            {
                vec3 T = normalize(intersection.surfaceTangent);
                vec3 N = normalize(intersection.surfaceNormal);
                vec3 B = cross(T, N) * intersection.bitangentSign;

                mappedNormal = _sample(normalMap).rgb;

                mappedNormal *= 2;
                mappedNormal -= 1;
                mappedNormal = normalize(T * mappedNormal.x + B * mappedNormal.y + N * mappedNormal.z);
            }

            float lightPDF = 1.0f;
            vec3 lighting = computeDirectLightingFromLights(rng, lightPDF, intersection.position, -mappedNormal/* looks correct but not sure why */, tMax);
            return intersection.surfaceColor * albedo * lighting + emissive + lights.ambientColor;
        } else {
            vec3 worldViewDir = direction;

            const mat3 rot = mat3(
                vec3(1.0, 0.0, 0.0),
                vec3(0.0, 0.0, -1.0),
                vec3(0.0, 1.0, 0.0)
            );
            vec3 skyboxRGB = texture(gSkybox3D, (rot) * worldViewDir).rgb;

            return skyboxRGB.rgb;
        }
    }
    #endif
}

void main() {
    vec4 outColorWorld;

    const ivec2 currentCoords = ivec2(gl_GlobalInvocationID);
    const ivec2 outputSize = imageSize(outDirectLightingImage);

    if(currentCoords.x >= outputSize.x
    || currentCoords.y >= outputSize.y) {
        return;
    }

    const vec2 inUV = vec2(currentCoords) / vec2(outputSize);

    // TODO: load directly
    float currDepth = texture(sampler2D(gDepth, nearestSampler), inUV).r;
    vec4 outReflections = vec4(0.0);

    float distanceToCamera;
    if(currDepth < 1.0) {
        RandomSampler rng;

        GBuffer gbuffer = unpackGBufferLight(inUV);
        if(gbuffer.metallicness <= 0.001f) {
            imageStore(outDirectLightingImage, currentCoords, vec4(0.0));
            return;
        }
        vec4 hWorldPos = cbo.inverseView * vec4(gbuffer.viewPosition, 1.0);
        vec3 worldPos = hWorldPos.xyz / hWorldPos.w;

        // TODO: store directly in CBO
        mat3 cboNormalView = transpose(inverse(mat3(cbo.view)));
        mat3 inverseNormalView = inverse(cboNormalView);
        vec3 normal = inverseNormalView * gbuffer.viewTBN[2];
        vec3 tangent = gbuffer.viewTBN[0];
        vec3 bitangent = gbuffer.viewTBN[1];

        initRNG(rng, inUV, push.frameWidth, push.frameHeight, push.frameCount);

        mat3 invTBN = inverse(mat3(tangent, bitangent, normal));
        mat3 TBN = transpose(invTBN);

#ifdef HARDWARE_SUPPORTS_RAY_TRACING
        const int SAMPLE_COUNT = 2; // TODO: more than one sample
        const float INV_SAMPLE_COUNT = 1.0f / SAMPLE_COUNT;

        [[dont_unroll]] for(int i = 0; i < SAMPLE_COUNT; i++) {
            vec3 gi;
            vec3 r;
            outReflections.rgb += calculateReflections(rng, gbuffer.roughness, worldPos, normal, TBN, invTBN, true);
        }
        outReflections.rgb *= INV_SAMPLE_COUNT;
#else
        outReflections.rgb = calculateReflections(rng, gbuffer.roughness, worldPos, normal, TBN, invTBN, false);
#endif
    } else {
        outReflections = vec4(0.0);
    }

    outReflections.a = 1.0;
    imageStore(outDirectLightingImage, currentCoords, outReflections);
}