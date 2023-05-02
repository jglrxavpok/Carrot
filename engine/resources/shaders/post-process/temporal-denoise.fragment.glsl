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

void main() {
    GBuffer gbuffer = unpackGBuffer(uv);
    vec4 currentFrameColor = texture(sampler2D(currentFrame, linearSampler), uv);
    currentFrameColor.a = 1.0;

    vec4 viewSpacePos = vec4(texture(sampler2D(currentViewPos, linearSampler), uv).rgb, 1.0);
    vec4 hWorldSpacePos = cbo.inverseView * viewSpacePos;

    vec4 prevNDC = cbo.projection * viewSpacePos;
    prevNDC.xyz /= prevNDC.w;
    prevNDC.xyz += gbuffer.motionVector;

    vec2 reprojectedUV = (prevNDC.xy + 1.0) / 2.0;
    vec4 previousViewSpacePos = texture(sampler2D(previousViewPos, linearSampler), reprojectedUV);
    vec4 hPreviousWorldSpacePos = previousFrameCBO.inverseView * previousViewSpacePos;

    float reprojected = float(length(hWorldSpacePos-hPreviousWorldSpacePos) <= 0.01);

    const float epsilon = 0.01;
    reprojected *= 1.0f - float(
        reprojectedUV.x <= epsilon || reprojectedUV.x >= 1-epsilon
        || reprojectedUV.y <= epsilon || reprojectedUV.y >= 1-epsilon
    );

    vec4 momentHistoryHistoryLength = texture(sampler2D(lastFrameMomentHistoryHistoryLength, linearSampler), reprojectedUV);
    vec4 previousFrameColor = texture(sampler2D(previousFrame, linearSampler), reprojectedUV);
    previousFrameColor.a = 1.0;

    float alpha = 0.8;
    //if(uv.x > 0.5)
    {
        if(isnan(previousFrameColor.x))
        {
            outColor = currentFrameColor;
        }
        else
        {
            outColor = reprojected * (mix(previousFrameColor, currentFrameColor, 1-alpha)) + (1.0f-reprojected) * currentFrameColor;
        }
    }

    float historyLength = momentHistoryHistoryLength.z * reprojected + 1.0;

    vec2 moments = vec2(luminance(outColor.rgb), 0.0);
    moments.y = moments.x * moments.x;

    vec4 result;
    if(historyLength <= 0.1f) {
        result = vec4(moments, historyLength, 1.0);
    } else {
        result = vec4(mix(momentHistoryHistoryLength.xy, moments, 1.0f / historyLength), historyLength, 1.0);
    }

    float variance = max(0.0f, result.g - result.r * result.r);
    outMomentHistoryHistoryLength = vec4(result.xyz, variance);
}