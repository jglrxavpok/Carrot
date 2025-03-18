#include <lighting/gi/types.glsl>
#define HARDWARE_SUPPORTS_RAY_TRACING 1 // TODO

// Trace rays to update world-space probes

layout(constant_id = 0) const uint LocalSizeX = 32;
layout(constant_id = 1) const uint LocalSizeY = 1;
const uint ScreenProbeSize = 8;
layout (local_size_x_id = 0) in;
layout (local_size_y_id = 1) in;

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

layout(set = 1, binding = 0, scalar) buffer ScreenProbes {
    ScreenProbe[] probes;
};
layout(set = 1, binding = 1, scalar) readonly buffer PreviousFrameScreenProbes {
    ScreenProbe[] previousFrameProbes;
};

layout(set = 1, binding = 2, scalar) buffer SpawnedProbes {
    uint spawnedProbeCount;
    uint[] spawnedProbes;
};

layout(set = 1, binding = 3, scalar) buffer EmptyProbes {
    uint count;
    uint[] indices; // index into 'spawnedProbes'
} emptyProbes;
layout(set = 1, binding = 4, scalar) buffer ReprojectedProbes {
    uint count;
    uint[] indices; // index into 'spawnedProbes'
} reprojectedProbes;
layout(set = 1, binding = 5, scalar) buffer SpawnedRays {
    uint spawnedRayCount;
    SpawnedRay[] spawnedRays;
};

uint packHalf(float f) {
    return packHalf2x16(vec2(f, 0));
}

// implementation based on AMD GI-1.0 paper
shared uint reprojectionScore;

const float probeCellSize = 1.0f;

uint getReprojectedProbeIndex(vec2 uv, vec2 motionVector) {
    const uint probesPerWidth = (push.frameWidth+ScreenProbeSize-1) / ScreenProbeSize;

    // reproject temporally, and check if pixel is reusable
    const vec2 reprojectedUV = uv + motionVector/2;
    const bool reprojectionInBounds = reprojectedUV.x >= 0.0f && reprojectedUV.x < 1.0f && reprojectedUV.y >= 0.0f && reprojectedUV.y < 1.0f;
    if(reprojectionInBounds) {
        const vec2 size = vec2(push.frameWidth, push.frameHeight);
        const ivec2 reprojectedPixel = ivec2(reprojectedUV * size);
        const uint reprojectedProbeIndex = reprojectedPixel.x / ScreenProbeSize + reprojectedPixel.y / ScreenProbeSize * probesPerWidth;
        return reprojectedProbeIndex;
    }

    return 0xFFFFu;
}

// KERNEL
void spawnScreenProbes() {
    reprojectionScore = (packHalf(65504.0f) << 16)/*score*/ | 0xFFFFu/*lane ID*/;
    barrier();

    // a single workgroup covers an entire tile, but there are as many groups as there are tiles.
    // this means the global invocation ID is pixel coords
    const uint probesPerWidth = (push.frameWidth+ScreenProbeSize-1) / ScreenProbeSize;
    const uint probeIndex = gl_WorkGroupID.x + gl_WorkGroupID.y * probesPerWidth;
    const ivec2 pixel = ivec2(gl_GlobalInvocationID.xy) + ivec2(push.frameCount*0)/*push constant alignment*/;
    const vec2 size = vec2(push.frameWidth, push.frameHeight);
    const vec2 uv = (vec2(pixel) + vec2(0.5)) / size;
    const GBuffer gbuffer = unpackGBuffer(uv);
    const ScreenProbe probe = probes[probeIndex];

    const mat3 cboNormalView = transpose(inverse(mat3(cbo.view)));
    const mat3 inverseNormalView = inverse(cboNormalView);
    const vec4 hWorldPosCurrentPixel = (cbo.inverseView * vec4(gbuffer.viewPosition, 1));
    const vec3 worldPosCurrentPixel = hWorldPosCurrentPixel.xyz / hWorldPosCurrentPixel.w;
    const vec3 normalCurrentPixel = inverseNormalView * gbuffer.viewTBN[2];

    // check if not sky
    if(gbuffer.albedo.a >= 0.001f) {
        const uint reprojectedProbeIndex = getReprojectedProbeIndex(uv, gbuffer.motionVector.xy);
        if(reprojectedProbeIndex != 0xFFFFu) {
            const ScreenProbe reprojectedProbe = previousFrameProbes[reprojectedProbeIndex];

            const vec3 dPos = reprojectedProbe.worldPos - worldPosCurrentPixel;
            const float planeDist = abs(dot(dPos, normalCurrentPixel));
            const float normalCheck = dot(normalCurrentPixel, reprojectedProbe.normal);
            if(planeDist < probeCellSize && normalCheck > 0.95) {
                const float dist = length(dPos);
                const uint probeScore = (packHalf(dist) << 16) | gl_LocalInvocationIndex;
                atomicMin(reprojectionScore, probeScore);
            }
        }

        // store score of best-matching pixel for this tile
    }

    barrier(); // at this point reprojectionScore contains the ID of the best matching pixel
    if(gl_LocalInvocationIndex != 0) {
        return;
    }
    uint reusedPixel = reprojectionScore & 0xFFFFu;
    const uint spawnedIndex = atomicAdd(spawnedProbeCount, 1);
    spawnedProbes[spawnedIndex] = probeIndex;

    if(reusedPixel == 0xFFFFu) {
        // no reuse
        probes[probeIndex].worldPos = worldPosCurrentPixel;
        probes[probeIndex].normal = normalCurrentPixel;
        probes[probeIndex].radiance = ivec3(0);
        probes[probeIndex].sampleCount = 0;
        probes[probeIndex].bestPixel = pixel;

        const uint emptyIndex = atomicAdd(emptyProbes.count, 1);
        //emptyProbes.indices[emptyIndex] = spawnedIndex;
        emptyProbes.indices[emptyIndex] = probeIndex;
    } else {
        // reuse
        const ivec2 screenTileStart = ivec2(gl_WorkGroupID.xy * uvec2(ScreenProbeSize));
        const uint localPixel = reprojectionScore & 0xFFFFu;
        const ivec2 posInTile = ivec2(localPixel % ScreenProbeSize, localPixel / ScreenProbeSize);
        const ivec2 reprojectedPixelPos = screenTileStart + posInTile;
        const vec2 reprojectedUV = (vec2(reprojectedPixelPos) + vec2(0.5)) / size;
        const GBuffer reprojectedGBuffer = unpackGBuffer(reprojectedUV);

        const uint reprojectedProbeIndex = getReprojectedProbeIndex(reprojectedUV, reprojectedGBuffer.motionVector.xy);
        const ScreenProbe reprojectedProbe = previousFrameProbes[reprojectedProbeIndex];

        probes[probeIndex].radiance = reprojectedProbe.radiance;
        probes[probeIndex].sampleCount = reprojectedProbe.sampleCount;
        probes[probeIndex].worldPos = (cbo.inverseView * vec4(reprojectedGBuffer.viewPosition, 1)).xyz;
        probes[probeIndex].normal = inverseNormalView * reprojectedGBuffer.viewTBN[2];
        probes[probeIndex].bestPixel = reprojectedPixelPos;

        const uint reprojectedIndex = atomicAdd(reprojectedProbes.count, 1);
        reprojectedProbes.indices[reprojectedIndex] = spawnedIndex;
    }
}

// KERNEL
void reorderSpawnedRays() {
    // bound checks
    if(reprojectedProbes.count == 0) {
        return;
    }

    const uint emptyProbeIndex = gl_GlobalInvocationID.x;
    if(emptyProbeIndex >= emptyProbes.count) {
        return;
    }

    uint probeIndex = emptyProbes.indices[emptyProbeIndex];
    // pick a reprojected probe to steal from
    uint indexToSteal = emptyProbeIndex % reprojectedProbes.count;

    // change which probe index the reprojected probe points to, and make it point to our empty probe
    atomicExchange(spawnedProbes[reprojectedProbes.indices[indexToSteal]], probeIndex);
}

// KERNEL
void spawnRays() {
    const uint MaxRaysPerProbe = 15;
    const uint spawnedProbeIndex = gl_GlobalInvocationID.x;

    if(spawnedProbeIndex >= spawnedProbeCount) {
        return;
    }

    const uint startRayIndex = atomicAdd(spawnedRayCount, MaxRaysPerProbe);
    const uint probeIndex = spawnedProbes[spawnedProbeIndex];
    for(int i = 0; i < MaxRaysPerProbe; i++) {
        spawnedRays[i + startRayIndex].probeIndex = probeIndex;
    }
}

// KERNEL
void traceRays() {
    uint rayIndex = gl_GlobalInvocationID.x;

    if(rayIndex >= spawnedRayCount) {
        return;
    }
    const uint probeIndex = spawnedRays[rayIndex].probeIndex;
    const ivec2 pixel = probes[probeIndex].bestPixel;
    uvec2 size = uvec2(push.frameWidth, push.frameHeight);

    if(pixel.x >= size.x || pixel.y >= size.y) {
        return;
    }

    vec2 uv = vec2(pixel) / size;
    GBuffer gbuffer = unpackGBuffer(uv);

    RandomSampler rng;
    initRNG(rng, uv, size.x, size.y, push.frameCount + rayIndex);

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
    vec3 rayDir = cosineSampleHemisphere(rng);
    rayDir = inverseNormalView * gbuffer.viewTBN * rayDir;

    vec3 brdf = vec3(1 / M_PI);

    intersection.hasIntersection = false;
    const float tMax = 50000.0f; /* TODO: specialization constant? compute properly?*/
    vec3 startPos = gWorldPos; // todo remove
    traceRayWithSurfaceInfo(intersection, startPos + rayDir * 0.001f, rayDir, tMax);
    vec3 radiance = vec3(0);
    if(intersection.hasIntersection) {
        // from "Raytraced reflections in 'Wolfenstein: Young Blood'":
        float mip = max(log2(3840.0 / push.frameWidth), 0.0);
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
        pbrInputsAtPoint.V = -rayDir;
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

        radiance += /*weakeningFactor * */brdf * (computeDirectLightingFromLights(rng, lightPDF, pbrInputsAtPoint, intersection.position, tMax) + gi + emissive);

        vec3 tangent;
        vec3 bitangent;
        computeTangents(mappedNormal, tangent, bitangent);
        const float jitterU = sampleNoise(rng) * 2 - 1;
        const float jitterV = sampleNoise(rng) * 2 - 1;
        float cellSize = giGetCellSize(intersection.position, cameraPos);
        const vec3 jitter = (tangent * jitterU + bitangent * jitterV) * cellSize;

        HashCellKey key;
        key.hitPosition = intersection.position + jitter;
        key.direction = -rayDir;
        key.cameraPos = cameraPos;
        key.rayLength = length(intersection.position - startPos);
        bool wasNew;
        uint index = hashGridInsert(CURRENT_FRAME, key, wasNew);
        if(index != InvalidCellIndex) {
            hashGridAdd(CURRENT_FRAME, index, key, radiance, 1);
            hashGridMark(CURRENT_FRAME, index, push.frameCount);
        }
    } else {
        const mat3 rot = mat3(
            vec3(1.0, 0.0, 0.0),
            vec3(0.0, 0.0, -1.0),
            vec3(0.0, 1.0, 0.0)
        );
        vec3 skyboxRGB = texture(gSkybox3D, (rot) * rayDir).rgb;

        radiance += /*weakeningFactor * */brdf * skyboxRGB.rgb;
    }

    // write to screen probe
    ivec3 added = packRadiance(radiance);
    atomicAdd(probes[probeIndex].radiance.x, added.x);
    atomicAdd(probes[probeIndex].radiance.y, added.y);
    atomicAdd(probes[probeIndex].radiance.z, added.z);
    atomicAdd(probes[probeIndex].sampleCount, 1);
#endif
}