// Removes all cells from the grid, to start a new frame

#extension GL_EXT_debug_printf : enable
#extension GL_EXT_buffer_reference2 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable

#define HASH_GRID_SET_ID 0
#include "hash-grid.include.glsl"

const uint LOCAL_SIZE = 256;
layout (local_size_x = LOCAL_SIZE) in;

layout(push_constant) uniform PushConstant {
    uint maxCellIndex;
    uint frame;
} push;

void main() {
    uint cellIndex = gl_GlobalInvocationID.x;

    if(cellIndex > push.maxCellIndex) {
        return;
    }

    if(cellIndex == 0) {
        hashGridResetCounters(CURRENT_FRAME);
    }

    //debugPrintfEXT("[%llu] test0 = %lx test1 = %lx\n", uint64_t(push.frame), uint64_t(grids[0]), uint64_t(grids[1]));

    hashGridClear(CURRENT_FRAME, cellIndex);
}