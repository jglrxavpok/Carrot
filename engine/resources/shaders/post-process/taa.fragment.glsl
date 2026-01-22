#include "includes/camera.glsl"
#include "includes/lighting_utils.glsl"
#include "includes/sampling.glsl"
#include <includes/gbuffer.glsl>
#include "includes/gbuffer_input.glsl"

layout(set = 0, binding = 0) uniform texture2D currentFrame;
layout(set = 0, binding = 1) uniform texture2D previousFrame;
layout(set = 0, binding = 2) uniform texture2D currentViewPos;
layout(set = 0, binding = 3) uniform texture2D previousViewPos;

layout(set = 0, binding = 4) uniform texture2D motionVectors_unused;
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
    /*float luminance = dot(color.rgb, vec3(0.299, 0.587, 0.114));
    float luminanceWeight = 1.0 / (1.0 + luminance);
    return vec4(color.rgb, 1.0) * luminanceWeight;*/
    return vec4(color.rgb, 1.0);
}

void main() {
    GBuffer gbuffer = unpackGBuffer(uv);
    vec4 currentFrameColor = AdjustHDRColor(texture(sampler2D(currentFrame, nearestSampler), uv));

    vec4 viewSpacePosH = texture(sampler2D(currentViewPos, nearestSampler), uv);
    bool isSkybox = viewSpacePosH.w <= 0.01f;

    if(isSkybox) {
        outColor = currentFrameColor;

        vec2 moments = vec2(luminance(currentFrameColor.rgb), 0.0);
        moments.y = moments.x * moments.x;

        outMomentHistoryHistoryLength = vec4(moments, 1.0f, 1.0f);
        outFirstSpatialDenoiseForNextFrame = outColor;
        return;
    }

    vec4 viewSpacePos = vec4(viewSpacePosH.rgb, 1.0);
    vec4 hWorldSpacePos = cbo.inverseView * viewSpacePos;

    vec4 prevNDC = cbo.jitteredProjection * viewSpacePos;
    prevNDC.xyz /= prevNDC.w;

    vec2 reprojectedUV = uv + gbuffer.motionVector.xy;
    vec4 previousViewSpacePos = vec4(texture(sampler2D(previousViewPos, nearestSampler), reprojectedUV).xyz, 1);
    vec4 hPreviousWorldSpacePos = previousFrameCBO.inverseView * previousViewSpacePos;

    float reprojected = exp(-distance(hPreviousWorldSpacePos.xyz, hWorldSpacePos.xyz) / 0.1);
    vec4 momentHistoryHistoryLength = texture(sampler2D(lastFrameMomentHistoryHistoryLength, nearestSampler), reprojectedUV);

    vec3 minColor = vec3(100000);
    vec3 maxColor = vec3(-100000);
    const vec2 textureDimensions = vec2(textureSize(sampler2D(currentFrame, linearSampler), 0));

    for(int x = -1; x <= 1; x++) {
        for(int y = -1; y <= 1; y++) {
            const vec2 dUV = vec2(x, y) / textureDimensions;
            vec4 color = AdjustHDRColor(texture(sampler2D(currentFrame, linearSampler), uv + dUV));
            minColor = min(minColor, color.rgb);
            maxColor = max(maxColor, color.rgb);
        }
    }
    vec4 previousFrameColor = AdjustHDRColor(texture(sampler2D(previousFrame, linearSampler), reprojectedUV));
    vec3 previousFrameColorClamped = clamp(previousFrameColor.rgb, minColor, maxColor);

    float historyLength = min(1000, momentHistoryHistoryLength.b * reprojected + 1.0);
    float alpha = 1.0f - 1.0f / historyLength;
    const float currentWeight = (1-alpha) * currentFrameColor.a;
    const float previousWeight = alpha * previousFrameColor.a;
    const float normalization = 1.0f / (currentWeight + previousWeight);

    if(isnan(previousFrameColor.x))
    {
        outColor = currentFrameColor;
    }
    else
    {
        //outColor = reprojected * (mix(previousFrameColor, currentFrameColor, 1-alpha)) + (1.0f-reprojected) * currentFrameColor;
        vec3 outRGB = (previousFrameColorClamped.rgb * previousWeight + currentFrameColor.rgb * currentWeight) * normalization;
        outColor = vec4(outRGB, 1.0);
    }

    vec2 moments = vec2(luminance(currentFrameColor.rgb), 0.0);
    moments.y = moments.x * moments.x;

    vec2 outMoments = (momentHistoryHistoryLength.xy * previousWeight + moments * currentWeight) * normalization;

    outMomentHistoryHistoryLength = vec4(outMoments, historyLength, 1.0f);
    outFirstSpatialDenoiseForNextFrame = vec4(0.0);
}