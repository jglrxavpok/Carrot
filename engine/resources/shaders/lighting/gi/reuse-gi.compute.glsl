#define HASH_GRID_SET_ID 0
#include "hash-grid.include.glsl"

const uint LOCAL_SIZE = 256;
layout (local_size_x = LOCAL_SIZE) in;

layout(push_constant) uniform PushConstant {
    uint maxCellIndex;
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

    // 3. add the two cells
    const vec3 currentValue = hashGridRead(CURRENT_FRAME, currentCellIndex);
    const vec3 previousValue = hashGridRead(PREVIOUS_FRAME, previousCellIndex);
    const vec3 v = currentValue + previousValue;
    const uint sampleCount = hashGridReadSampleCount(PREVIOUS_FRAME, previousCellIndex)+hashGridReadSampleCount(CURRENT_FRAME, currentCellIndex);

    // 4. Store new value
    hashGridWrite(CURRENT_FRAME, currentCellIndex, key, v, sampleCount);

    // 5. increase age of cell?
}