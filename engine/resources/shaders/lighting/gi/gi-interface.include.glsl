#include "hash-grid.include.glsl"
#include <includes/rng.glsl>

struct GIInputs {
    vec3 hitPosition;
    vec3 cameraPosition;
    vec3 incomingRay;
    vec3 surfaceNormal;
    float metallic;
    vec3 surfaceColor;
    float roughness;
    uint frameIndex;
};

#define USE_GI 1

vec3 GetOrComputeRayResult(inout RandomSampler rng, in GIInputs giInput) {
#if USE_GI
    bool wasNew;
    HashCellKey cellDesc;

    vec3 tangent;
    vec3 bitangent;
    computeTangents(giInput.surfaceNormal, tangent, bitangent);
    const float jitterU = sampleNoise(rng) * 2 - 1;
    const float jitterV = sampleNoise(rng) * 2 - 1;

    float cellSize = 0.135f; // TODO: adaptive cell size
    const vec3 jitter = (tangent * jitterU + bitangent * jitterV) * cellSize;

    cellDesc.hitPosition = giInput.hitPosition + jitter;
    cellDesc.direction = giInput.incomingRay;

    uint cellIndex = hashGridInsert(CURRENT_FRAME, cellDesc, wasNew);

    if(cellIndex == InvalidCellIndex) {
        // hash grid is full :(
        return vec3(0,0,0);
    }

    // newly touched cell (for this frame), ask for an update
    bool firstTouch = hashGridMark(CURRENT_FRAME, cellIndex, giInput.frameIndex);
    if(wasNew) {
        hashGridMarkForUpdate(CURRENT_FRAME, cellIndex, cellDesc, giInput.surfaceNormal, giInput.metallic, giInput.roughness, giInput.surfaceColor);
    }

    uint previousFrameCellIndex = hashGridFind(PREVIOUS_FRAME, cellDesc);
    if(previousFrameCellIndex == InvalidCellIndex) {
        return vec3(0.0, 0, 0);
    }

    return hashGridRead(PREVIOUS_FRAME, previousFrameCellIndex);
#else
    return vec3(0.0);
#endif
}

vec3 GetGINoUpdate(in GIInputs giInput) {
#if USE_GI
    bool wasNew;
    HashCellKey cellDesc;
    cellDesc.hitPosition = giInput.hitPosition;
    cellDesc.direction = giInput.incomingRay;

    uint previousFrameCellIndex = hashGridFind(PREVIOUS_FRAME, cellDesc);
    if(previousFrameCellIndex == InvalidCellIndex) {
        return vec3(0.0, 0, 0);
    }

    return hashGridRead(PREVIOUS_FRAME, previousFrameCellIndex);
#else
    return vec3(0.0);
#endif
}
