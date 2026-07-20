static const int LocalSizeX = 32;
static const int LocalSizeY = 32;
groupshared float3[LocalSizeY][LocalSizeX] neighbors;

float3 readColor(Texture2D<float4> input, uint2 globalCoords, uint2 localCoords, int2 offset) {
    int2 groupPos = int2(localCoords) + offset;
    if (groupPos.x < 0 || groupPos.y < 0 || groupPos.x >= LocalSizeX || groupPos.y >= LocalSizeY) {
        return input.Load(int3(int2(globalCoords) + offset, 0)).rgb;
    }
    return neighbors[groupPos.y][groupPos.x];
}

[shader("compute")]
[numthreads(LocalSizeX, LocalSizeY, 1)]
void rejectFireflies(
    uniform Texture2D<float4> input,
    uniform RWTexture2D<float4> output,
    uint2 coords: SV_DispatchThreadID,
    uint2 localCoords: SV_GroupThreadID
) {
    const float3 pixelColor = input.Load(uint3(coords, 0)).rgb;
    neighbors[localCoords.y][localCoords.x] = pixelColor;
    GroupMemoryBarrierWithGroupSync();

    const int radius = 1;
    float3 sum = pixelColor; // Done in RGB, should this be HSV?
    float3 sumOfSquares = pixelColor * pixelColor;
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            if (dx == 0 && dy == 0)
                continue;
            float3 color = readColor(input, coords, localCoords, int2(dx, dy));
            sum += color;
            sumOfSquares += color * color;
        }
    }

    const float sampleCount = (radius * 2 + 1) * (radius * 2 + 1);
    sum /= sampleCount; // becomes mean
    sumOfSquares /= sampleCount;

    const float3 variance = sumOfSquares - sum * sum;
    const float3 average = sum;

    // Based on https://alain.xyz/blog/ray-tracing-filtering#variance-estimation
    const float3 deviation = sqrt(max(1e-5, variance));
    const float3 highThreshold = average + deviation * 0;
    const float3 overflow = max(0, pixelColor - highThreshold);
    const float3 result = pixelColor - overflow;

    output.Store(coords, float4(result, 1));
}