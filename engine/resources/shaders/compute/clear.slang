[shader("compute")]
[numthreads(1, 1, 1)]
void clearSingleInt(uniform uint64_t address) {
    uint *pTarget = (uint *)address;
    *pTarget = 0;
}

[shader("compute")]
[numthreads(32, 32, 1)]
void clearU64Image(
    uniform RWTexture2D<uint64_t> image, 
    uint3 coords: SV_DispatchThreadID) {
    uint w, h;
    image.GetDimensions(w, h);
    if (coords.x >= w || coords.y >= h) {
        return;
    }

    image[coords.xy] = 0;
}

[shader("compute")]
[numthreads(32, 32, 1)]
void clearRGBAImage(
    uniform RWTexture2D<float4> image, 
    uint3 coords: SV_DispatchThreadID) {
    uint w, h;
    image.GetDimensions(w, h);
    if (coords.x >= w || coords.y >= h) {
        return;
    }

    image[coords.xy] = 0;
}