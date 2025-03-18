#define HASH_GRID_SET_ID 0
#include "hash-grid.include.glsl"

#include <includes/materials.glsl>
MATERIAL_SYSTEM_SET(1)

#include <includes/rng.glsl>

const uint MAX_REUSE = 5;
const uint LOCAL_SIZE = 256;
layout (local_size_x = LOCAL_SIZE) in;
layout (local_size_y = 1) in;

layout(push_constant) uniform PushConstant {
    uint maxCellIndex;
    uint frameCount;
    uint64_t pLastTouchedFrame;
    uint64_t pCells;
};

layout(buffer_reference, scalar) buffer BufferToUint {
    uint v;
};

void decayCells() {
    uint cellIndex = gl_GlobalInvocationID.x;

    if(cellIndex >= maxCellIndex) {
        return;
    }

    const uint decayTime = 10;
    if(BufferToUint(pLastTouchedFrame+cellIndex*4).v+decayTime < frameCount) {
        hashGridClear(HashGrid(pCells), cellIndex);
    }
}

void reuseCells() {
    const uint previousCellIndex = gl_GlobalInvocationID.x;

    if(previousCellIndex > maxCellIndex) {
        return;
    }

    // 1. get key & value of cell at cellIndex on previous frame's grid
    HashCellKey key = hashGridGetKey(PREVIOUS_FRAME, previousCellIndex);
    if(grids[PREVIOUS_FRAME].hash2[previousCellIndex] == 0) {
        return;
    }
    vec3 previousSample = hashGridRead(PREVIOUS_FRAME, previousCellIndex);
    uint previousSampleCount = hashGridGetSampleCount(PREVIOUS_FRAME, previousCellIndex);
    vec3 currentSample;
    uint currentSampleCount;

    // 2. get corresponding cell in current frame
    bool wasNew;
    uint currentCellIndex = hashGridInsert(CURRENT_FRAME, key, wasNew);
    if(wasNew) {
        // no such cell in current frame, will just copy it

        currentSample = vec3(0);
        currentSampleCount = 0;
        hashGridMark(CURRENT_FRAME, currentCellIndex, frameCount);
    } else {
        currentSample = hashGridRead(CURRENT_FRAME, currentCellIndex);
        currentSampleCount = hashGridGetSampleCount(CURRENT_FRAME, currentCellIndex);
    }

    if(currentCellIndex == InvalidCellIndex) {
        return; // no more room
    }

    RandomSampler rng;
    initRNG(rng, vec2(currentCellIndex, previousCellIndex) / maxCellIndex, maxCellIndex, maxCellIndex, frameCount);

    // 3. reuse samples from previous frame
    uint totalSampleCount = min(10000, currentSampleCount+previousSampleCount);

    vec3 combined;
    float combinedSampleCount;
    if(previousSampleCount == 0) {
        if(currentSampleCount == 0) {
            combined = previousSample / previousSampleCount;
            combinedSampleCount = previousSampleCount;
        } else {
            combined = vec3(0);
            combinedSampleCount = 1;
        }
    } else {
        if(currentSampleCount == 0) {
            combined = previousSample / previousSampleCount;
            combinedSampleCount = previousSampleCount;
        } else {
            vec3 newSample = currentSample / currentSampleCount + previousSample / previousSampleCount;
            float alpha = max(0.05f, 1.0f / totalSampleCount);
            combinedSampleCount = 1.0f / alpha;
            combined = mix(newSample, currentSample / currentSampleCount, alpha);
        }
    }

    combined *= combinedSampleCount;

    if(dot(combined, combined) >= 100000*100000) {
        combined = vec3(10000,0,0);
    }
    // TODO: is this sample count correct?
    hashGridWrite(CURRENT_FRAME, currentCellIndex, key, combined, uint(combinedSampleCount));
}