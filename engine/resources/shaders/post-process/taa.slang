// Implementation based on https://www.elopezr.com/temporal-aa-and-the-quest-for-the-holy-trail/

import modules.gbuffer;
import modules.viewport;

ParameterBlock<GBufferInputs> gBufferInputs;

struct IO {
    RWTexture2D<float4> output;
    Texture2D<float4> currentFrameColor;
    Texture2D<float4> accumulationBuffer;
    Texture2D<float4> momentsAndHistory;

    SamplerState linearSampler;
    SamplerState nearestSampler;
};
ParameterBlock<IO> io;
ParameterBlock<Viewport> viewport;

float2 getVelocity(float2 pixelCenter) {

    float closestDepth = 100;
    float2 closestDepthCoords = 0;

    for(int x = -1; x <= 1; x++) {
        for(int y = -1; y <= 1; y++) {
            const float2 neighborPos = (pixelCenter + float2(x, y)) / viewport.frameDimensions;
            const GBuffer neighborGBuffer = gBufferInputs.read(neighborPos);
            
            if(neighborGBuffer.readDepth < closestDepth) {
                closestDepth = neighborGBuffer.readDepth;
                closestDepthCoords = neighborPos;
            }
        }
    }

    return gBufferInputs.read(closestDepthCoords).motionVector.xy;
}

[shader("compute")]
[numthreads(8,8,1)]
void main(uint2 coords: SV_DispatchThreadID) {
    if(any(coords >= viewport.frameDimensions)) {
        return;
    }

    const float2 pixelCenter = coords + 0.5f;
    const float4 originalColor = io.currentFrameColor.Load(int3(coords, 0));
    const float2 uv = pixelCenter / float2(viewport.frameDimensions);
    
    const int2 previousCoords = pixelCenter + getVelocity(pixelCenter) * float2(viewport.frameDimensions);

    float alpha = 0.1f;
    const float4 previousColor = io.accumulationBuffer.Load(int3(previousCoords, 0));

    if(any(previousCoords >= viewport.frameDimensions) || any(previousCoords < 0)) {
        io.output.Store(coords, originalColor);

        return;
    }

    // Neighbor clamping
    float4 minColor = 10000.0f;
    float4 maxColor = -10000.0f;
    for(int x = -1; x <= 1; x++) {
        for(int y = -1; y <= 1; y++) {
            const float2 neighborPos = pixelCenter + float2(x, y);
            const float4 neighborColor = io.currentFrameColor.Load(int3(neighborPos, 0));
            minColor = min(neighborColor, minColor);
            maxColor = max(neighborColor, maxColor);
        }
    }

    const float3 previousColorClamped = clamp(previousColor, minColor, maxColor).rgb;

    float3 finalColor = previousColorClamped * (1.0f-alpha) + originalColor.rgb*alpha;

    io.output.Store(coords, float4(finalColor, 1));
}