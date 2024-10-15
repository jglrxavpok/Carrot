// Removes all cells from the grid, to start a new frame

#include "hash-grid.include.glsl"

const uint LOCAL_SIZE = 256;
layout (local_size_x = LOCAL_SIZE) in;

layout(push_constant) uniform PushConstant {
    uint maxCellIndex;
};

void main() {
    uint cellIndex = gl_GlobalInvocationID.x;

    if(cellIndex > maxCellIndex) {
        return;
    }

    hashGridClear(CURRENT_FRAME, cellIndex);
}