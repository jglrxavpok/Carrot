#include "includes/camera.glsl"
#include "includes/sampling.glsl"

layout(set = 0, binding = 0) uniform texture2D currentFrame;
layout(set = 0, binding = 1) uniform texture2D previousFrame;
layout(set = 0, binding = 2) uniform texture2D currentViewPos;
layout(set = 0, binding = 3) uniform texture2D previousViewPos;

layout(set = 0, binding = 4) uniform texture2D motionVectors;
layout(set = 0, binding = 5) uniform texture2D lastFrameMomentHistoryHistoryLength;

layout(set = 0, binding = 6) uniform sampler nearestSampler;
layout(set = 0, binding = 7) uniform sampler linearSampler;


DEFINE_CAMERA_SET(1)

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outMomentHistoryHistoryLength;

void main() {
    vec4 currentFrameColor = texture(sampler2D(currentFrame, linearSampler), uv);
    currentFrameColor.a = 1.0;

    vec4 viewSpacePos = vec4(texture(sampler2D(currentViewPos, linearSampler), uv).rgb, 1.0);
    vec4 hWorldSpacePos = cbo.inverseView * viewSpacePos;

    vec4 prevNDC = cbo.projection * viewSpacePos;
    prevNDC.xyz /= prevNDC.w;
    prevNDC.xyz += texture(sampler2D(motionVectors, linearSampler), uv).xyz;

    vec2 reprojectedUV = (prevNDC.xy + 1.0) / 2.0;
    vec4 previousViewSpacePos = texture(sampler2D(previousViewPos, linearSampler), reprojectedUV);
    vec4 hPreviousWorldSpacePos = previousFrameCBO.inverseView * previousViewSpacePos;

    float reprojected = float(length(hWorldSpacePos-hPreviousWorldSpacePos) <= 0.01);

    const float epsilon = 0.01;
    reprojected *= 1.0f - float(
        reprojectedUV.x <= epsilon || reprojectedUV.x >= 1-epsilon
        || reprojectedUV.y <= epsilon || reprojectedUV.y >= 1-epsilon
    );

    vec4 momentHistoryHistoryLength = texture(sampler2D(lastFrameMomentHistoryHistoryLength, nearestSampler), reprojectedUV);
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
    outMomentHistoryHistoryLength = vec4(momentHistoryHistoryLength.xy * reprojected, historyLength, 1.0);
}