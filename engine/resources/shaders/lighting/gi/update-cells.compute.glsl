#define HARDWARE_SUPPORTS_RAY_TRACING 1 // TODO

// Trace rays to update world-space probes

const uint LOCAL_SIZE = 256;
layout (local_size_x = LOCAL_SIZE) in;

layout(push_constant) uniform PushConstant {
    bool hasTLAS;
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

#include "hash-grid.include.glsl"

#include <lighting/rt.include.glsl>

void main() {
    uint updateIndex = gl_GlobalInvocationID.x;
    if(updateIndex >= grids[CURRENT_FRAME].updateCount) {
        return;
    }

    CellUpdate update = grids[CURRENT_FRAME].pUpdates.v[updateIndex];
    uint cellIndex = update.cellIndex;
    const vec3 startPos = update.key.hitPosition;
    const vec3 direction = update.key.direction;
    vec3 alreadyPresentRadiance = hashGridRead(CURRENT_FRAME, cellIndex);

    vec3 newRadiance = vec3(0.0); // TODO
#ifdef HARDWARE_SUPPORTS_RAY_TRACING
    {
        const float tMax = 5000.0f; /* TODO: specialization constant? compute properly?*/

        intersection.hasIntersection = false;

        traceRayWithSurfaceInfo(intersection, startPos, direction, tMax);
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

            PbrInputs pbrInputsAtPoint;
            pbrInputsAtPoint.alpha = 1.0f;// TODO: fetch from material
            pbrInputsAtPoint.baseColor = intersection.surfaceColor;
            pbrInputsAtPoint.metallic = 0.0f;// TODO: fetch from material
            pbrInputsAtPoint.V = -direction;
            pbrInputsAtPoint.N = -mappedNormal;// looks correct but not sure why
            pbrInputsAtPoint.NdotV = abs(dot(pbrInputsAtPoint.V, pbrInputsAtPoint.N));

            RandomSampler rng;
            newRadiance = computeDirectLightingFromLights(rng, lightPDF, pbrInputsAtPoint, intersection.position, tMax);
        } else {
            newRadiance = vec3(0.0); // TODO: sample from env map
        }
    }
#endif

    float alpha = 0.5f;
    vec3 averagedSample = mix(newRadiance, alreadyPresentRadiance, alpha);
    hashGridWrite(CURRENT_FRAME, cellIndex, averagedSample);
}