#include "hash-grid.include.glsl"

struct GIInputs {
    vec3 hitPosition;
    vec3 cameraPosition;
    vec3 incomingRay;
    uint frameIndex;
};

vec3 GetOrComputeRayResult(in GIInputs giInput) {
    #if 0
    return vec3(0.0);
    #else
    bool wasNew;
    HashCellKey cellDesc;
    cellDesc.hitPosition = giInput.hitPosition;
    cellDesc.direction = giInput.incomingRay;

    uint cellIndex = hashGridInsert(CURRENT_FRAME, cellDesc, wasNew);

    if(cellIndex == InvalidCellIndex) {
        // hash grid is full :(
        return vec3(0.0);
    }

    // newly touched cell (for this frame), ask for an update
    bool firstTouch = hashGridMark(CURRENT_FRAME, cellIndex, giInput.frameIndex);
    if(wasNew || firstTouch) {
        hashGridMarkForUpdate(CURRENT_FRAME, cellIndex, cellDesc);
    }

    uint previousFrameCellIndex = hashGridFind(PREVIOUS_FRAME, cellDesc);
    if(previousFrameCellIndex == InvalidCellIndex) {
        return vec3(0.0);
    }

    return hashGridRead(PREVIOUS_FRAME, previousFrameCellIndex);
    #endif
}