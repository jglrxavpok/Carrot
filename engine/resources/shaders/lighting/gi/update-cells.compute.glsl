#define HARDWARE_SUPPORTS_RAY_TRACING 1 // TODO

// Trace rays to update world-space probes

const uint LOCAL_SIZE = 32;
layout (local_size_x = LOCAL_SIZE) in;
layout (local_size_y = LOCAL_SIZE) in;

layout(push_constant) uniform PushConstant {
    uint frameCount;
    uint frameWidth;
    uint frameHeight;

#ifdef HARDWARE_SUPPORTS_RAY_TRACING
    bool hasTLAS; // handle special cases where no raytraceable geometry is present in the scene
#endif
} push;

#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int32 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require

#include <includes/materials.glsl>
#include <includes/lights.glsl>
MATERIAL_SYSTEM_SET(2)
LIGHT_SET(3)
#include <includes/rng.glsl>
#include <lighting/brdf.glsl>
#include <lighting/base.common.glsl>

#include <includes/gbuffer.glsl>
#include "includes/gbuffer_input.glsl"
#include <includes/camera.glsl>
DEFINE_GBUFFER_INPUTS(4)
DEFINE_CAMERA_SET(5)
#include "includes/gbuffer_unpack.glsl"

#ifdef HARDWARE_SUPPORTS_RAY_TRACING
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_ray_query : enable
#endif

#define HASH_GRID_SET_ID 0
#include "gi-interface.include.glsl"

#include <lighting/rt.include.glsl>

void main() {
    uvec2 pixel = gl_GlobalInvocationID.xy;

    uvec2 size = uvec2(push.frameWidth, push.frameHeight);
    if(pixel.x >= size.x || pixel.y >= size.y) {
        return;
    }

    vec2 uv = vec2(pixel) / size;
    GBuffer gbuffer = unpackGBuffer(uv);

    RandomSampler rng;
    initRNG(rng, uv, size.x, size.y, push.frameCount);

    vec3 gAlbedo = gbuffer.albedo.rgb;
    vec3 gEmissive = gbuffer.emissiveColor.rgb;
    vec4 hWorldPos = cbo.inverseView * vec4(gbuffer.viewPosition, 1.0);
    vec3 gWorldPos = hWorldPos.xyz / hWorldPos.w;

    // TODO: store directly in CBO
    mat3 cboNormalView = transpose(inverse(mat3(cbo.view)));
    mat3 inverseNormalView = inverse(cboNormalView);
    vec3 gNormal = inverseNormalView * gbuffer.viewTBN[2];
    vec3 gTangent = inverseNormalView * gbuffer.viewTBN[0];
    vec2 gMetallicRoughness = vec2(gbuffer.metallicness, gbuffer.roughness);

    vec4 hCameraPos = cbo.inverseView * vec4(0,0,0,1);
    vec3 cameraPos = hCameraPos.xyz / hCameraPos.w;
    vec3 directionFromCamera = normalize(gWorldPos - cameraPos);

#ifdef HARDWARE_SUPPORTS_RAY_TRACING
    // specular part
    const int MAX_SAMPLES = 1;// TODO: not working?
    const int MAX_BOUNCES = 3; // TODO: spec constant

    vec3 totalRadiance = vec3(0.0);
    float totalWeight = 0.0f;

    vec3 bounceRadiance[MAX_BOUNCES] = {
        vec3(0),
        vec3(0),
        vec3(0),
    };
    uint bounceCellIndices[MAX_BOUNCES] = {
        InvalidCellIndex,
        InvalidCellIndex,
        InvalidCellIndex,
    };
    HashCellKey bounceKeys[MAX_BOUNCES];

    for(int sampleIndex = 0; sampleIndex < MAX_SAMPLES; sampleIndex++) {
        vec3 newRadiance = vec3(0.0);
        vec3 emissiveColor = gEmissive;
        vec3 albedo = gAlbedo;
        vec3 startPos = gWorldPos;
        vec3 incomingRay = directionFromCamera;
        float metallic = gMetallicRoughness.x;
        float roughness = gMetallicRoughness.y;
        vec3 surfaceNormal = gNormal;
        vec3 surfaceTangent = gTangent;
        if(isnan(dot(surfaceNormal, surfaceNormal))) {
            break;
        }
        if(isnan(dot(surfaceTangent, surfaceTangent))) {
            break;
        }
        if(isnan(dot(roughness, roughness))) {
            break;
        }
        if(isnan(dot(metallic, metallic))) {
            break;
        }
        if(isnan(dot(incomingRay, incomingRay))) {
            break;
        }

        vec3 beta = vec3(1.0);

        float weight = 1.0f;
        vec3 previousPos = cameraPos;
        for(int bounceIndex = 0; bounceIndex < MAX_BOUNCES; bounceIndex++) {
            vec3 tangent;
            vec3 bitangent;
            computeTangents(-incomingRay, tangent, bitangent);
            const float jitterU = sampleNoise(rng) * 2 - 1;
            const float jitterV = sampleNoise(rng) * 2 - 1;
            float cellSize = giGetCellSize(startPos, cameraPos);
            const vec3 jitter = (tangent * jitterU + bitangent * jitterV) * cellSize;

            bounceKeys[bounceIndex].hitPosition = startPos + jitter;
            bounceKeys[bounceIndex].direction = incomingRay;
            bounceKeys[bounceIndex].cameraPos = cameraPos;
            bounceKeys[bounceIndex].rayLength = length(startPos - previousPos);

            previousPos = startPos;
            bool stopHere = false;
            bool wasNew;
            vec3 currentBounceRadiance = vec3(0.0);
            bounceCellIndices[bounceIndex] = hashGridInsert(CURRENT_FRAME, bounceKeys[bounceIndex], wasNew);

            // diffuse part
            PbrInputs pbr;
            vec3 V = -incomingRay;
            vec3 N = normalize(surfaceNormal);
            pbr.alpha = roughness * roughness;
            pbr.metallic = metallic;
            pbr.baseColor = albedo;
            pbr.V = V;
            pbr.N = N;
            pbr.NdotV = abs(dot(N, V));

            // todo: move out of loop
            {
                currentBounceRadiance += emissiveColor + lights.ambientColor;
                const float MAX_LIGHT_DISTANCE = 5000.0f; /* TODO: specialization constant? compute properly?*/

                float lightPDF = 1.0f;
                vec3 lightAtPoint = computeDirectLightingFromLights(/*inout*/rng, /*inout*/lightPDF, pbr, startPos, MAX_LIGHT_DISTANCE);
                currentBounceRadiance += lightAtPoint * lightPDF;
            }

            // reflective part
            vec3 specularDir = importanceSampleDirectionForReflection(rng, incomingRay, surfaceNormal, roughness);
            pbr.L = specularDir;
            pbr.H = normalize(pbr.L + pbr.H);
            computeDotProducts(pbr);

            weight *= max(0.01,abs(dot(surfaceNormal, specularDir)));

            vec3 brdf = glTF_BRDF_WithImportanceSampling(pbr);
            if(isnan(brdf.x)) {
                break;
            }
            const float tMax = 5000.0f; /* TODO: specialization constant? compute properly?*/

            intersection.hasIntersection = false;
            traceRayWithSurfaceInfo(intersection, startPos, specularDir, tMax);
            if(intersection.hasIntersection) {
                // from "Raytraced reflections in 'Wolfenstein: Young Blood'":
                float mip = 0;// TODO? max(log2(3840.0 / push.frameWidth), 0.0);
                vec2 f;
                float viewDistance = 0.0;// TODO: eye position? length(worldPos - rayOrigin);
                float hitDistance = length(intersection.position - startPos);
                f.x = viewDistance;// ray origin
                f.y = hitDistance;// ray length
                f = clamp(f / tMax, 0, 1);
                f = sqrt(f);
                mip += f.x * 10.0f;
                mip += f.y * 10.0f;

                #define _sample(TYPE) textureLod(sampler2D(textures[materials[intersection.materialIndex].TYPE], linearSampler), intersection.uv, floor(mip))

                vec3 albedo = _sample(albedo).rgb;
                vec3 emissive = _sample(emissive).rgb;
                vec2 metallicRoughness = _sample(metallicRoughness).xy;

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

                float roughnessAtPoint = metallicRoughness.y;
                PbrInputs pbrInputsAtPoint;
                pbrInputsAtPoint.alpha = roughnessAtPoint * roughnessAtPoint;
                pbrInputsAtPoint.baseColor = intersection.surfaceColor;
                pbrInputsAtPoint.metallic = metallicRoughness.x;
                pbrInputsAtPoint.V = -specularDir;
                pbrInputsAtPoint.N = mappedNormal;// looks correct but not sure why
                pbrInputsAtPoint.NdotV = abs(dot(pbrInputsAtPoint.V, pbrInputsAtPoint.N));

                GIInputs giInputs;
                giInputs.hitPosition = intersection.position;
                giInputs.cameraPosition = cameraPos;
                giInputs.startOfRay = startPos;
                giInputs.surfaceNormal = mappedNormal;
                giInputs.metallic = pbrInputsAtPoint.metallic;
                giInputs.roughness = roughnessAtPoint;
                giInputs.frameIndex = push.frameCount;
                vec3 gi = giGetNoUpdate(rng, giInputs);

                startPos = intersection.position;
                incomingRay = -specularDir;
                surfaceNormal = giInputs.surfaceNormal;
                roughness = giInputs.roughness;
                albedo = pbrInputsAtPoint.baseColor;
                metallic = giInputs.metallic;

                // todo: update beta
                //beta *= brdf * albedo.rgb;

                currentBounceRadiance += brdf * (computeDirectLightingFromLights(rng, lightPDF, pbrInputsAtPoint, intersection.position, tMax) + gi);
            } else {
                const mat3 rot = mat3(
                    vec3(1.0, 0.0, 0.0),
                    vec3(0.0, 0.0, -1.0),
                    vec3(0.0, 1.0, 0.0)
                );
                vec3 skyboxRGB = texture(gSkybox3D, (rot) * specularDir).rgb;

                currentBounceRadiance +=  skyboxRGB.rgb * brdf;
                stopHere = true;
            }

            for(int otherBounce = bounceIndex; otherBounce >= 0; otherBounce--) {
                bounceRadiance[bounceIndex] += currentBounceRadiance;
            }
            newRadiance += beta * currentBounceRadiance;

            if(stopHere) {
                break;
            }
        }

        if(weight > 0.001f) {
            totalRadiance += newRadiance/* * weight*/;
            totalWeight += weight;
        }
    }

    for(int bounceIndex = 0; bounceIndex < MAX_BOUNCES; bounceIndex++) {
        uint bounceCellIndex = bounceCellIndices[bounceIndex];
        if(bounceCellIndex == InvalidCellIndex) {
            continue;
        }
        hashGridMark(CURRENT_FRAME, bounceCellIndex, push.frameCount);
        hashGridAdd(CURRENT_FRAME, bounceCellIndex, bounceKeys[bounceIndex], bounceRadiance[bounceIndex], MAX_SAMPLES);
    }
#endif
}