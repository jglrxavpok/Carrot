#define HARDWARE_SUPPORTS_RAY_TRACING 1 // TODO

// Trace rays to update world-space probes

const uint LOCAL_SIZE = 256;
layout (local_size_x = LOCAL_SIZE) in;

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

#ifdef HARDWARE_SUPPORTS_RAY_TRACING
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_ray_query : enable
#endif

#define HASH_GRID_SET_ID 0
#include "gi-interface.include.glsl"

layout(set = 1, binding = 0) uniform samplerCube gSkybox3D;

#include <lighting/rt.include.glsl>

void main() {
    uint updateIndex = gl_GlobalInvocationID.x;

    uint maxIndex = grids[CURRENT_FRAME].updateCount;
    if(updateIndex >= maxIndex) {
        return;
    }

    CellUpdate update = grids[CURRENT_FRAME].pUpdates.v[updateIndex];
    uint cellIndex = update.cellIndex;
    #if 1
    vec3 startPos = update.key.hitPosition;
    vec3 incomingRay = update.key.direction; // TODO: should be *incoming* direction
    float metallic = update.metallic;
    float roughness = update.roughness;
    vec3 surfaceNormal = update.surfaceNormal;
    vec3 surfaceTangent = cross(vec3(1,0,0), update.surfaceNormal);
    uint sampleCount = hashGridReadSampleCount(CURRENT_FRAME, cellIndex);
    vec3 alreadyPresentRadiance = hashGridRead(CURRENT_FRAME, cellIndex);

    RandomSampler rng;
    initRNG(rng, startPos.xy, push.frameWidth, push.frameHeight, push.frameCount);

    vec3 newRadiance = vec3(0.0);
    vec3 emissiveColor = vec3(0.0); // TODO
    vec3 albedo = vec3(1.0); // TODO


#ifdef HARDWARE_SUPPORTS_RAY_TRACING
    // specular part
    {
        vec3 beta = vec3(1.0);

        const int MAX_BOUNCES = 3; // TODO: spec constant
        for(int bounceIndex = 0; bounceIndex < MAX_BOUNCES; bounceIndex++) {
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

            {
                newRadiance += beta * (emissiveColor + lights.ambientColor);
                const float MAX_LIGHT_DISTANCE = 5000.0f; /* TODO: specialization constant? compute properly?*/

                float lightPDF = 1.0f;
                vec3 lightAtPoint = computeDirectLightingFromLights(/*inout*/rng, /*inout*/lightPDF, pbr, startPos, MAX_LIGHT_DISTANCE);
                newRadiance += beta * lightAtPoint * lightPDF;
            }

            // reflective part
            vec3 specularDir = importanceSampleDirectionForReflection(rng, incomingRay, surfaceNormal, roughness);
            pbr.L = specularDir;
            pbr.H = normalize(pbr.L + pbr.H);
            computeDotProducts(pbr);

            vec3 brdf = glTF_BRDF_WithImportanceSampling(pbr);
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

                float roughnessAtPoint = 1.0f; // TODO: fetch from material
                PbrInputs pbrInputsAtPoint;
                pbrInputsAtPoint.alpha = roughnessAtPoint * roughnessAtPoint;
                pbrInputsAtPoint.baseColor = intersection.surfaceColor;
                pbrInputsAtPoint.metallic = 0.0f;// TODO: fetch from material
                pbrInputsAtPoint.V = -specularDir;
                pbrInputsAtPoint.N = mappedNormal;// looks correct but not sure why
                pbrInputsAtPoint.NdotV = abs(dot(pbrInputsAtPoint.V, pbrInputsAtPoint.N));

                GIInputs giInputs;
                giInputs.hitPosition = intersection.position;
                giInputs.cameraPosition = startPos; // is this valid?
                giInputs.incomingRay = specularDir;
                giInputs.surfaceNormal = mappedNormal;
                giInputs.metallic = pbrInputsAtPoint.metallic;
                giInputs.roughness = roughnessAtPoint;
                giInputs.frameIndex = push.frameCount;
                newRadiance += beta * brdf * computeDirectLightingFromLights(rng, lightPDF, pbrInputsAtPoint, intersection.position, tMax);

                startPos = intersection.position;
                incomingRay = -specularDir;
                surfaceNormal = giInputs.surfaceNormal;
                roughness = giInputs.roughness;
                albedo = pbrInputsAtPoint.baseColor;
                metallic = giInputs.metallic;

                // todo: update beta
            } else {
                const mat3 rot = mat3(
                vec3(1.0, 0.0, 0.0),
                vec3(0.0, 0.0, -1.0),
                vec3(0.0, 1.0, 0.0)
                );
                vec3 skyboxRGB = texture(gSkybox3D, (rot) * specularDir).rgb;

                newRadiance +=  skyboxRGB.rgb * brdf;
                break;
            }
        }
    }
#endif

    vec3 averagedSample = alreadyPresentRadiance+newRadiance;
    hashGridWrite(CURRENT_FRAME, cellIndex, update.key, averagedSample, 1);
    #endif
}