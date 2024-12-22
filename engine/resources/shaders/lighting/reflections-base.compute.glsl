#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int32 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable

#include <includes/lights.glsl>
#include <includes/materials.glsl>
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

#include <lighting/brdf.glsl>
#include "includes/rng.glsl"
#include <lighting/gi/gi-interface.include.glsl>

layout(rgba32f, set = 5, binding = 0) uniform writeonly image2D outDirectLightingImage;

// needs to be included after LIGHT_SET macro & RT data
#include <lighting/base.common.glsl>
#include <lighting/rt.include.glsl>

vec3 calculateReflections(inout RandomSampler rng, vec3 albedo, float metallic, float roughness, vec3 worldPos, vec3 normal, mat3 TBN, mat3 invTBN, bool raytracing) {
    const vec3 cameraPos = (cbo.inverseView * vec4(0, 0, 0, 1)).xyz;

    vec3 toCamera = worldPos - cameraPos;
    vec3 incomingRay = normalize(toCamera);

    vec3 direction = importanceSampleDirectionForReflection(rng, incomingRay, normal, roughness);

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

        vec3 V = -incomingRay;
        vec3 L = normalize(direction);
        vec3 N = normalize(normal);
        vec3 H = normalize(L+V);

        PbrInputs pbr;
        pbr.alpha = roughness * roughness;
        pbr.metallic = metallic;
        pbr.baseColor = albedo;
        pbr.V = V;
        pbr.L = L;
        pbr.N = N;
        pbr.H = H;
        computeDotProducts(pbr);
        vec3 brdf = glTF_BRDF_WithImportanceSampling(pbr);

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

            float roughness = 1.0f; // TODO: fetch from material
            PbrInputs pbrInputsAtPoint;
            pbrInputsAtPoint.alpha = roughness*roughness;
            pbrInputsAtPoint.baseColor = intersection.surfaceColor * albedo;
            pbrInputsAtPoint.metallic = 0.0f; // TODO: fetch from material
            pbrInputsAtPoint.V = -direction;
            pbrInputsAtPoint.N = -mappedNormal; // looks correct but not sure why
            pbrInputsAtPoint.NdotV = abs(dot(pbrInputsAtPoint.V, pbrInputsAtPoint.N));

            vec3 lighting = computeDirectLightingFromLights(rng, lightPDF, pbrInputsAtPoint, intersection.position, tMax);

            GIInputs giInputs;
            giInputs.hitPosition = intersection.position;
            giInputs.cameraPosition = cameraPos;
            giInputs.incomingRay = direction;
            giInputs.frameIndex = push.frameCount;
            giInputs.surfaceNormal = mappedNormal;
            giInputs.metallic = pbrInputsAtPoint.metallic;
            giInputs.roughness = roughness;
            giInputs.surfaceColor = pbrInputsAtPoint.baseColor;

            vec3 gi = giGetNoUpdate(giInputs);
            vec3 lightColor = (lighting + emissive + gi);
            return lightColor * brdf;
        } else {
            vec3 worldViewDir = direction;

            const mat3 rot = mat3(
                vec3(1.0, 0.0, 0.0),
                vec3(0.0, 0.0, -1.0),
                vec3(0.0, 1.0, 0.0)
            );
            vec3 skyboxRGB = texture(gSkybox3D, (rot) * worldViewDir).rgb;

            return skyboxRGB.rgb * brdf;
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
    if(currDepth < 1.0f) {
        RandomSampler rng;

        GBuffer gbuffer = unpackGBuffer(inUV);
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
        vec3 tangent = inverseNormalView * gbuffer.viewTBN[0];
        vec3 bitangent = inverseNormalView * gbuffer.viewTBN[1];

        initRNG(rng, inUV, push.frameWidth, push.frameHeight, push.frameCount);

        mat3 TBN = transpose(inverse(mat3(tangent, bitangent, normal)));
        mat3 invTBN = inverse(TBN);

#ifdef HARDWARE_SUPPORTS_RAY_TRACING
        const int SAMPLE_COUNT = 4;
        const float INV_SAMPLE_COUNT = 1.0f / SAMPLE_COUNT;

        [[dont_unroll]] for(int i = 0; i < SAMPLE_COUNT; i++) {
            vec3 gi;
            vec3 r;
            outReflections.rgb += calculateReflections(rng, gbuffer.albedo.rgb, gbuffer.metallicness, gbuffer.roughness, worldPos, normal, TBN, invTBN, true);
        }
        outReflections.rgb *= INV_SAMPLE_COUNT;
#else
        outReflections.rgb = calculateReflections(rng, gbuffer.albedo.rgb, gbuffer.metallicness, gbuffer.roughness, worldPos, normal, TBN, invTBN, false);
#endif
    } else {
        outReflections = vec4(0.0);
    }

    outReflections.a = 1.0;
    imageStore(outDirectLightingImage, currentCoords, outReflections);
}