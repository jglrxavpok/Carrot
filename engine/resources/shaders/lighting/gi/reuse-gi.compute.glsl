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

void main() {
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
    uint reservoirCount = hashGridGetReservoirCount(CURRENT_FRAME, currentCellIndex);
    int actualReuse = int(floor(reservoirCount * sampleNoise(rng)));
    int offset = int(floor(sampleNoise(rng) * reservoirCount));
    for(int i = 0; i < MAX_RESERVOIRS; i++) {
        const int reservoirIndex = (offset + i) % MAX_RESERVOIRS;
        const Reservoir preExistingReservoir = hashGridGetReservoir(PREVIOUS_FRAME, previousCellIndex, reservoirIndex);
        const float u1 = sampleNoise(rng);
        const float u2 = sampleNoise(rng);

        hashGridCombine(CURRENT_FRAME, currentCellIndex, i, u1, u2, preExistingReservoir);
    }
}