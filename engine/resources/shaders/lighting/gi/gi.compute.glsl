#define HASH_GRID_SET_ID 0
#include "hash-grid.include.glsl"

#include <includes/materials.glsl>
MATERIAL_SYSTEM_SET(1)

#include <includes/rng.glsl>

const uint MAX_REUSE = 5;
const uint LOCAL_SIZE = 256;
layout (local_size_x = LOCAL_SIZE) in;

layout(push_constant) uniform PushConstant {
    uint maxCellIndex;
    uint frameCount;
};

void clearCells() {
    uint cellIndex = gl_GlobalInvocationID.x;

    if(cellIndex == 0) {
        hashGridResetCounters(CURRENT_FRAME);
    }

    if(cellIndex < maxCellIndex) {
        hashGridClear(CURRENT_FRAME, cellIndex);
    }
}

void decayCells() {
    uint cellIndex = gl_GlobalInvocationID.x;

    if(cellIndex >= maxCellIndex) {
        return;
    }

    const uint decayTime = 30;
    if(grids[CURRENT_FRAME].pLastTouchedFrame.v[cellIndex]+decayTime < frameCount) {
        hashGridClear(CURRENT_FRAME, cellIndex);
    }
}

void reuseCells() {
    const uint currentCellIndex = gl_GlobalInvocationID.x;

    if(currentCellIndex > maxCellIndex) {
        return;
    }

    // 1. get key & value of cell at cellIndex on current frame's grid
    HashCellKey key = hashGridGetKey(CURRENT_FRAME, currentCellIndex);

    // 2. get corresponding cell in current frame
    uint previousCellIndex = hashGridFind(PREVIOUS_FRAME, key);
    if(previousCellIndex == InvalidCellIndex) {
        return;
    }

    RandomSampler rng;
    initRNG(rng, vec2(currentCellIndex, previousCellIndex) / maxCellIndex, maxCellIndex, maxCellIndex, frameCount);

    // 3. reuse samples from previous frame
    vec3 currentSample = hashGridRead(CURRENT_FRAME, currentCellIndex);
    vec3 previousSample = hashGridRead(PREVIOUS_FRAME, previousCellIndex);
    uint currentSampleCount = hashGridGetSampleCount(CURRENT_FRAME, currentCellIndex);
    uint previousSampleCount = hashGridGetSampleCount(PREVIOUS_FRAME, previousCellIndex);
    uint totalSampleCount = min(10000, currentSampleCount+previousSampleCount);

    vec3 combined;
    float combinedSampleCount;
    if(previousSampleCount == 0) {
        combined = currentSample / currentSampleCount;
        combinedSampleCount = currentSampleCount;
    } else {
        vec3 newSample = currentSample / currentSampleCount + previousSample / previousSampleCount;
        combined = mix(newSample, currentSample / currentSampleCount, 1.0f / totalSampleCount);
    }

    combined *= totalSampleCount;
    combinedSampleCount = totalSampleCount;

    if(dot(combined, combined) >= 100000*100000) {
        combined = vec3(10000,0,0);
    }
    // TODO: is this sample count correct?
    hashGridWrite(CURRENT_FRAME, currentCellIndex, key, combined, uint(combinedSampleCount));
}