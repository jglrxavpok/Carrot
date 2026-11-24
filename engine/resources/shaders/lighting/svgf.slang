// Implementation based on https://research.nvidia.com/sites/default/files/pubs/2017-07_Spatiotemporal-Variance-Guided-Filtering%3A//svgf_preprint.pdf

import glsl;
import modules.base;
import modules.gbuffer;
import modules.compute_fullscreen;
import modules.camera;

[[vk::binding(0, 0)]]
ParameterBlock<Camera> camera;

[[vk::binding(0, 1)]]
ParameterBlock<GBufferInputs> previousFrameGBufferInputs;

[[vk::binding(0, 2)]]
ParameterBlock<GBufferInputs> currentFrameGBufferInputs;

[[vk::binding(1, 3)]]
RWTexture2D<float4> currentFrameColor;
[[vk::binding(2, 3)]]
RWTexture2D<float4> colorHistory;
[[vk::binding(3, 3)]]
RWTexture2D<float4> momentsHistory;

[[vk::binding(4, 3)]]
RWTexture2D<float4> colorOutput;
[[vk::binding(5, 3)]]
RWTexture2D<float4> momentsOutput; // Encodes history length in 3rd channel // TODO: use proper type

[[vk::binding(6, 3)]]
Texture2D<float4> depthDepthGradientAndNormal;

[[vk::binding(7, 3)]]
RWTexture2D<float4> varianceOutput;

struct Stats {
    uint64_t totalTime;
    uint64_t getCurrentPixel;
    uint64_t blurVariance;
    uint64_t export;

    uint64_t singlePixel[25];
    uint64_t singlePixelFetchInfo[25];
    uint64_t singlePixelFetchColor[25];
    uint64_t singlePixelApplyWeights[25];
};
[[vk::binding(0, 4)]]
RWStructuredBuffer<Stats> stats;

#define PROFILE 0

struct Sample {
    float4 color;
    float2 moments;
}

// tap filter around given uv, where depth, normals & entity ID must match
Optional<Sample> tapFilter(float2 uv, float2 imageDimensions, float expectedDepth, float3 expectedWorldNormal, uint32_t4 expectedEntityID) {
    float4 color = 0;
    float2 moments = 0;
    int count = 0;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            const float2 newUV = uv + float2(dx, dy) / imageDimensions;

            // check that pixel is reusable
            if (newUV.x < 0 || newUV.y < 0 || newUV.x >= 1 || newUV.y >= 1) {
                continue;
            }

            const PixelInfo pixelInfo = PixelInfo(camera.PreviousFrame(), previousFrameGBufferInputs, newUV);

            // TODO: get near/far to compute depth in meters
            if (abs(pixelInfo.gBuffer.viewPosition.z - expectedDepth) >= 0.1f) {
                continue;
            }

            if (dot(pixelInfo.worldNormal, expectedWorldNormal) < 0.9) {
                continue;
            }

            if (expectedEntityID != pixelInfo.gBuffer.entityID) {
                continue;
            }

            // reuse pixel
            color += colorHistory.Load(int2(newUV * imageDimensions));
            moments += momentsHistory.Load(int2(newUV * imageDimensions)).xy;
            count++;
        }
    }

    if (count > 0) {
        return Sample(color / count, moments / count);
    }
    return none;
}

[shader("compute")]
[numthreads(8,8,1)]
void temporal(uint2 coords: SV_DispatchThreadID) {
    uint w;
    uint h;
    currentFrameColor.GetDimensions(w, h);
    if (coords.x >= w || coords.y >= h) {
        return;
    }

    const float2 uv = (coords + 0.5f) / float2(w, h);
    const PixelInfo pixelInfo = PixelInfo(camera.CurrentFrame(), currentFrameGBufferInputs, uv);
    if (pixelInfo.isSky) {
        colorOutput.Store(coords, float4(0, 0, 0, 0));
        momentsOutput.Store(coords, float4(0, 0, 0, 1));
        return;
    }

    const float4 currentSample = currentFrameColor.Load(coords);
    const float currentLuminance = luminance(currentSample.rgb);
    const float2 currentMoments = float2(currentLuminance, currentLuminance * currentLuminance);
    Optional<Sample> optSample = tapFilter(uv + pixelInfo.gBuffer.motionVector, float2(w, h), pixelInfo.gBuffer.viewPosition.z, pixelInfo.worldNormal, pixelInfo.gBuffer.entityID);
    float4 previousColor = 0;
    float2 previousMoments = 0;
    float historyLength = momentsHistory.Load(coords).z;
    if (let previousSample = optSample) {
        previousColor = previousSample.color;
        previousMoments = previousSample.moments;
        historyLength++;
    } else {
        previousColor = currentSample;
        previousMoments = currentMoments;
        historyLength = 0;
    }

    const float alpha = 0.2f;//historyLength > 0 ? 1.0f / historyLength : 0;
    float4 colorResult = previousColor * (1 - alpha) + alpha * currentSample;
    float2 momentsResult = previousMoments * (1 - alpha) + alpha * currentMoments;

    if (isnan(colorResult.x)) {
        colorResult = float4(0, 0, 1, 1);
    }
    colorOutput.Store(coords, float4(colorResult.rgb, 1));
    momentsOutput.Store(coords, float4(momentsResult, historyLength, 1));
}

// fast pow of value n to fixed exponent
double powNormal128(double a) {
    const double a2 = a * a;
    const double a4 = a2 * a2;
    const double a8 = a4 * a4;
    const double a16 = a8 * a8;
    const double a32 = a16 * a16;
    const double a64 = a32 * a32;
    const double a128 = a64 * a64;
    return a128;
}

float weightFunc(float variance, float depthDivisor, float3 centerColor, float centerDepth, float3 centerNormal, float3 otherColor, float otherDepth, float3 otherNormal) {
    const float sigmaDepth = 16;
    const float sigmaNormal = 128;
    const float sigmaLuminance = 4;

    const float depthNumerator = abs(otherDepth - centerDepth);
    const float depthDenominator = sigmaDepth * depthDivisor + 10e-12f;
    const float weightDepth = exp(-depthNumerator / depthDenominator);
    const float weightNormal = max(0.0f, float(powNormal128(double(dot(centerNormal, otherNormal)))));

    const float luminanceNumerator = abs(luminance(centerColor.rgb) - luminance(otherColor.rgb));
    const float luminanceDenominator = (sigmaLuminance * sqrt(variance)) + 10e-12f;
    const float weightLuminance = exp(-luminanceNumerator / luminanceDenominator);

    // TODO: same mesh ID
    return weightNormal * weightLuminance * weightDepth;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void estimateVariance(uint2 coords: SV_DispatchThreadID) {
    uint w;
    uint h;
    varianceOutput.GetDimensions(w, h);
    if (coords.x >= w || coords.y >= h) {
        return;
    }

    float3 momentsAndHistoryLength = momentsHistory.Load(coords).xyz;
    if (momentsAndHistoryLength.z < 4) {
        // Not enough history, estimate spatially
        const float2 centerUV = (float2(coords) + 0.5f) / float2(w, h);
        const PixelInfo centerPixelInfo = PixelInfo(camera.CurrentFrame(), currentFrameGBufferInputs, centerUV);
        const float4 centerColor = currentFrameColor.Load(coords);
        const float centerLuminanceValue = luminance(centerColor.rgb);

        float2 moments = float2(0,0);
        float weightSum = 0.0f;

        const float4 depthDepthGradientAndNormalValue = depthDepthGradientAndNormal.Load(int3(coords, 0));
        const float centerDepth = depthDepthGradientAndNormalValue.x;
        const float depthGradient = depthDepthGradientAndNormalValue.y;
        const float3 centerNormal = unpackNormal(depthDepthGradientAndNormalValue.zw);

        for (int dy = -3; dy <= 3; dy++) {
            for (int dx = -3; dx <= 3; dx++) {
                int2 newCoords = int2(coords) + int2(dx, dy);
                if (any(newCoords < 0) || newCoords.x >= w || newCoords.y >= h) {
                    continue;
                }

                const float3 color = currentFrameColor.Load(newCoords).rgb;
                const float luminanceValue = luminance(color.rgb);

                const PixelInfo otherPixelInfo = PixelInfo(camera.CurrentFrame(), currentFrameGBufferInputs, (newCoords + 0.5f) / float2(w, h));
                if (otherPixelInfo.isSky)
                    continue;

                const float4 otherDepthDepthGradientAndNormalValue = depthDepthGradientAndNormal.Load(int3(newCoords, 0));
                const float otherDepth = otherDepthDepthGradientAndNormalValue.x;
                const float3 otherNormal = unpackNormal(otherDepthDepthGradientAndNormalValue.zw);

                const float weight = weightFunc(1, depthGradient, centerColor.rgb, centerDepth, centerNormal, color, otherDepth, otherNormal);
                const float l = luminanceValue;
                moments += float2(l, l * l) * weight;
                weightSum += weight;
            }
        }

        if (weightSum > 0.0001f) {
            moments /= weightSum;
        }
        const float variance = max(0, moments.y - moments.x * moments.x);
        varianceOutput.Store(coords, float4(centerColor.rgb, variance));
    } else {
        // otherwise, integrate temporally
        const float4 centerColor = currentFrameColor.Load(coords);
        const float variance = max(0, momentsAndHistoryLength.y - momentsAndHistoryLength.x * momentsAndHistoryLength.x);
        varianceOutput.Store(coords, float4(centerColor.rgb, variance));
    }
}

float getDepth(int2 coords, int dx, int dy, int w, int h) {
    const int2 otherCoords = int2(coords) + int2(dx, dy);
    if (otherCoords.x < 0 || otherCoords.y < 0 || otherCoords.x >= w || otherCoords.y >= h) {
        return 0;
    }
    PixelInfo pixel = PixelInfo(camera.CurrentFrame(), currentFrameGBufferInputs, otherCoords / float2(w, h));
    return pixel.gBuffer.viewPosition.z;
}

// writes to depthDepthGradientAndNormal
[shader("pixel")]
float4 computeDepthGradientAndNormals(in float2 uv) {
    const float depth = currentFrameGBufferInputs.viewPos.SampleGrad(currentFrameGBufferInputs.gNearestSampler, uv, 0, 0).z;
    const float depthGradient = max(abs(ddx(depth)), abs(ddy(depth)));
    const float2 packedNormal = currentFrameGBufferInputs.readPackedNormal(uv);
    return float4(depth, depthGradient, packedNormal);
}

[shader("compute")]
[numthreads(8, 8, 1)]
void waveletFilter(uint2 coords: SV_DispatchThreadID, uniform uint32_t iterationIndex) {
#if PROFILE
    uint64_t headerStart = clockARB();
    defer InterlockedAdd(stats[0].totalTime, clockARB() - headerStart);
#endif

    uint w;
    uint h;
    colorOutput.GetDimensions(w, h);
    if (coords.x >= w || coords.y >= h) {
        return;
    }

#if PROFILE
    uint64_t getCurrentPixelStart = clockARB();
#endif

    const float2 uv = (coords + 0.5f) / float2(w, h);
    const PixelInfo centerPixel = PixelInfo(camera.CurrentFrame(), currentFrameGBufferInputs, uv);
    if (centerPixel.isSky) {
        colorOutput.Store(coords, float4(0,0,0,1));
        return;
    }

#if PROFILE
    InterlockedAdd(stats[0].getCurrentPixel, clockARB() - getCurrentPixelStart);
#endif

    int jumpSize = 1 << iterationIndex;

    const float kernel[3] = { 3 / 8.0f, 1 / 4.0f, 1 / 16.0f };
    const float4 centerData = currentFrameColor.Load(coords);
    const float3 centerColor = centerData.rgb;
    const float centerVariance = centerData.a;

    float weightSum = 0.0f;
    float4 accumulatedColor = 0;

#if PROFILE
    uint64_t blurVarianceStart = clockARB();
#endif

    float gaussianBlurVariance = 0;
    int c = 0;
    float gaussianWeightSum = 0;
    float gaussianKernel[2] = { 1 / 2.0f, 1 / 4.0f };
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            const int2 otherCoords = int2(coords) + int2(dx, dy);
            if (otherCoords.x < 0 || otherCoords.y < 0 || otherCoords.x >= w || otherCoords.y >= h) {
                continue;
            }

            const float w = gaussianKernel[abs(dx)] * gaussianKernel[abs(dy)];
            gaussianBlurVariance += currentFrameColor.Load(otherCoords).w * w;
            gaussianWeightSum += w;
        }
    }
    if (gaussianWeightSum > 0)
        gaussianBlurVariance /= gaussianWeightSum;

#if PROFILE
    InterlockedAdd(stats[0].blurVariance, clockARB() - blurVarianceStart);
#endif

    const float4 depthDepthGradientAndNormalValue = depthDepthGradientAndNormal.Load(int3(coords, 0));
    const float centerDepth = depthDepthGradientAndNormalValue.x;
    const float depthGradient = depthDepthGradientAndNormalValue.y;
    const float3 centerNormal = unpackNormal(depthDepthGradientAndNormalValue.zw);

    int pixelIndex = 0;
    [loop] for (int dy = -2; dy <= 2; dy++) {
        [loop] for (int dx = -2; dx <= 2; dx++) {
            int currentPixelIndex = pixelIndex++;

#if PROFILE
            uint64_t singlePixelStart = clockARB();
            defer InterlockedAdd(stats[0].singlePixel[currentPixelIndex], clockARB() - singlePixelStart);
#endif

            const int2 otherCoords = int2(coords) + int2(dx, dy) * jumpSize;
            if (otherCoords.x < 0 || otherCoords.y < 0 || otherCoords.x >= w || otherCoords.y >= h) {
                continue;
            }

            const float2 otherUV = otherCoords / float2(w, h);

#if PROFILE
            uint64_t fetchInfoStart = clockARB();
#endif
            const PixelInfo otherPixel = PixelInfo(camera.CurrentFrame(), currentFrameGBufferInputs, otherUV);

            const float4 otherDepthDepthGradientAndNormalValue = depthDepthGradientAndNormal.Load(int3(otherCoords, 0));
            const float otherDepth = otherDepthDepthGradientAndNormalValue.x;
            const float3 otherNormal = unpackNormal(otherDepthDepthGradientAndNormalValue.zw);

#if PROFILE
            InterlockedAdd(stats[0].singlePixelFetchInfo[currentPixelIndex], clockARB() - fetchInfoStart);
#endif

            if (otherPixel.isSky)
                continue;

#if PROFILE
            uint64_t fetchColorStart = clockARB();
#endif
            const float4 otherData = currentFrameColor.Load(otherCoords);
            const float3 otherColor = otherData.rgb;

#if PROFILE
            InterlockedAdd(stats[0].singlePixelFetchColor[currentPixelIndex], clockARB() - fetchColorStart);
#endif

#if PROFILE
            uint64_t applyWeightsStart = clockARB();
#endif

            float weight = kernel[abs(dx)] * kernel[abs(dy)];
            weight *= weightFunc(gaussianBlurVariance, depthGradient, centerColor, centerDepth, centerNormal, otherColor, otherDepth, otherNormal);
            accumulatedColor += float4(weight.xxx, weight * weight) * otherData;
            weightSum += weight;
#if PROFILE
            InterlockedAdd(stats[0].singlePixelApplyWeights[currentPixelIndex], clockARB() - applyWeightsStart);
#endif
        }
    }

#if PROFILE
    uint64_t exportStart = clockARB();
#endif

    if (weightSum > 0.00001f) {
        accumulatedColor /= float4(weightSum.xxx, weightSum * weightSum); // W contains variance
    }
    colorOutput.Store(coords, accumulatedColor);

#if PROFILE
    InterlockedAdd(stats[0].export, clockARB() - exportStart);
#endif
}