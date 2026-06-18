module harfbuzz;

#include "harfbuzz/hb_gpu_base.slang"
#include "harfbuzz/hb_gpu_draw.slang"
#include "harfbuzz/hb_gpu_paint.slang"
#include "harfbuzz/hb_gpu_vertex.slang"

public void harfbuzzDilateVertex(inout float2 position, inout float2 texcoord,
                         float2 normal, float4 jac,
                         float4x4 m, float2 viewport) {
    hb_gpu_dilate(position, texcoord, normal, jac, m, viewport);
}

public float4 harfbuzzPaint(float2 renderCoord, uint glyphLoc_, float4 foreground,
                    out float coverage) {
    return hb_gpu_paint(renderCoord, glyphLoc_, foreground, coverage);
}