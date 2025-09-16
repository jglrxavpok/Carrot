import denoise_base;

export struct Configuration : IConfiguration {
    float4 swizzle(float4 rgba) {
        return rgba.rrrr;
    }

    float4 convertToFinalFormat(float4 swizzled) {
        return float4(swizzled.rrrr);
    }
}

[shader("compute")]
[numthreads(LOCAL_SIZE_X, LOCAL_SIZE_Y, 1)]
void main(
    uniform uint iterationIndex,
    uint3 coords: SV_DispatchThreadID,
    uint3 localCoords : SV_GroupThreadID

) {
    denoise(iterationIndex, coords, localCoords);
}