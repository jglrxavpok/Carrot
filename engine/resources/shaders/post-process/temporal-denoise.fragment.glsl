#include "includes/camera.glsl"
#include "includes/lighting_utils.glsl"
#include "includes/sampling.glsl"
#include "includes/gbuffer_input.glsl"

layout(set = 0, binding = 0) uniform texture2D currentFrame;
layout(set = 0, binding = 1) uniform texture2D previousFrame;
layout(set = 0, binding = 2) uniform texture2D currentViewPos;
layout(set = 0, binding = 3) uniform texture2D previousViewPos;

layout(set = 0, binding = 4) uniform texture2D motionVectors;
layout(set = 0, binding = 5) uniform texture2D lastFrameMomentHistoryHistoryLength;

layout(set = 0, binding = 6) uniform sampler nearestSampler;
layout(set = 0, binding = 7) uniform sampler linearSampler;


DEFINE_CAMERA_SET(1)

DEFINE_GBUFFER_INPUTS(2)
#include "includes/gbuffer_unpack.glsl"

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outMomentHistoryHistoryLength;
layout(location = 2) out vec4 outFirstSpatialDenoiseForNextFrame;

vec4 AdjustHDRColor(vec4 color)
{
    float luminance = dot(color.rgb, vec3(0.299, 0.587, 0.114));
    float luminanceWeight = 1.0 / (1.0 + luminance);
    return vec4(color.rgb, 1.0) * luminanceWeight;
}

void main() {
    GBuffer gbuffer = unpackGBuffer(uv);
    vec4 currentFrameColor = texture(sampler2D(currentFrame, linearSampler), uv);
    vec4 adjustedCurrent = AdjustHDRColor(currentFrameColor);
    //currentFrameColor.a = 1.0;

    vec4 viewSpacePos = vec4(texture(sampler2D(currentViewPos, linearSampler), uv).rgb, 1.0);
    vec4 hWorldSpacePos = cbo.inverseView * viewSpacePos;

    vec4 prevNDC = cbo.nonJitteredProjection * viewSpacePos;
    prevNDC.xyz /= prevNDC.w;
    prevNDC.xyz += gbuffer.motionVector;

    vec2 reprojectedUV = (prevNDC.xy + 1.0) / 2.0;
    vec4 previousViewSpacePos = texture(sampler2D(previousViewPos, linearSampler), reprojectedUV);
    vec4 hPreviousWorldSpacePos = previousFrameCBO.inverseView * previousViewSpacePos;

    float reprojected = 1.0f;//float(length(hWorldSpacePos-hPreviousWorldSpacePos) <= 0.01); // distance clip

    const float epsilon = 0.01;
/*    reprojected *= 1.0f - float(
        reprojectedUV.x <= epsilon || reprojectedUV.x >= 1-epsilon
        || reprojectedUV.y <= epsilon || reprojectedUV.y >= 1-epsilon
    );*/

    //reprojectedUV = clamp(reprojectedUV, vec2(epsilon), vec2(1-epsilon));

    vec4 momentHistoryHistoryLength = texture(sampler2D(lastFrameMomentHistoryHistoryLength, linearSampler), reprojectedUV);

    vec4 minColor = vec4(100000);
    vec4 maxColor = vec4(-100000);
    const ivec2 size = textureSize(sampler2D(currentFrame, linearSampler), 0);

    for(int x = -1; x <= 1; x++) {
        for(int y = -1; y <= 1; y++) {
            const vec2 dUV = vec2(x, y) / size;
            vec4 color = texture(sampler2D(currentFrame, linearSampler), reprojectedUV + dUV);
            minColor = min(minColor, color);
            maxColor = max(maxColor, color);
        }
    }
    vec4 previousFrameColor = texture(sampler2D(previousFrame, linearSampler), reprojectedUV);
    vec4 adjustedPrevious = AdjustHDRColor(clamp(previousFrameColor, minColor, maxColor));
    //vec4 adjustedPrevious = AdjustHDRColor(previousFrameColor);
    //previousFrameColor.a = 1.0;

    const float colorClampStrength = 200.0f;
    //reprojected *= exp(-length(currentFrameColor.rgb - previousFrameColor.rgb) / colorClampStrength); // color clamp
//    reprojected = 1.0;

    float historyLength = momentHistoryHistoryLength.b * reprojected + 1.0;
    float alpha = 0.9f;//historyLength <= 1.1 ? 0.9f : 1.0f - 1.0f / historyLength;
    //float alpha = 0.9f;
    const float currentWeight = (1-alpha) * adjustedCurrent.a;
    const float previousWeight = alpha * adjustedPrevious.a;
    const float normalization = 1.0f / (currentWeight + previousWeight);

    //if(uv.x > 0.5)
    {
        if(isnan(previousFrameColor.x))
        {
            outColor = currentFrameColor;
        }
        else
        {
            //outColor = reprojected * (mix(previousFrameColor, currentFrameColor, 1-alpha)) + (1.0f-reprojected) * currentFrameColor;
            vec3 outRGB = (previousFrameColor.rgb * previousWeight + currentFrameColor.rgb * currentWeight) * normalization;
            outColor = vec4(outRGB, 1.0);
        }
    }

    vec2 moments = vec2(luminance(currentFrameColor.rgb), 0.0);
    moments.y = moments.x * moments.x;


    vec2 outMoments = (momentHistoryHistoryLength.xy * previousWeight + moments * currentWeight) * normalization;

    outMomentHistoryHistoryLength = vec4(outMoments, historyLength, 1.0f);
    outFirstSpatialDenoiseForNextFrame = vec4(0.0);
}