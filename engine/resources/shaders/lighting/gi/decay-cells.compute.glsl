// Decay cells inside hash grids and clear those which have expired

#include "hash-grid.include.glsl"

const uint LOCAL_SIZE = 256;
layout (local_size_x = LOCAL_SIZE) in;

layout(push_constant) uniform PushConstant {
    uint maxCellIndex;
    uint frameID;
};

void main() {
    uint cellIndex = gl_GlobalInvocationID.x;

    if(cellIndex > maxCellIndex) {
        return;
    }

    const uint decayTime = 30;
    if(grids[CURRENT_FRAME].pLastTouchedFrame.v[cellIndex]+decayTime < frameID) {
        hashGridClear(CURRENT_FRAME, cellIndex);
    }
}