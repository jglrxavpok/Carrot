#include "hash-grid.include.glsl"

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

vec3 GetOrComputeRayResult(in GIInputs giInput) {
#if USE_GI
    bool wasNew;
    HashCellKey cellDesc;
    cellDesc.hitPosition = giInput.hitPosition;
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

    return hashGridRead(PREVIOUS_FRAME, previousFrameCellIndex) / hashGridReadSampleCount(PREVIOUS_FRAME, previousFrameCellIndex);
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

    return hashGridRead(PREVIOUS_FRAME, previousFrameCellIndex) / hashGridReadSampleCount(PREVIOUS_FRAME, previousFrameCellIndex);
#else
    return vec3(0.0);
#endif
}
