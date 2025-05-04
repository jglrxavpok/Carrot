const uint LOCAL_SIZE_X = 16;
const uint LOCAL_SIZE_Y = 8;
layout (local_size_x = LOCAL_SIZE_X) in;
layout (local_size_y = LOCAL_SIZE_Y) in;

#include "includes/camera.glsl"
#include "includes/lighting_utils.glsl"
#include "includes/sampling.glsl"
#include <includes/gbuffer.glsl>
#include "includes/gbuffer_input.glsl"

DEFINE_GBUFFER_INPUTS(0)
DEFINE_CAMERA_SET(1)

#include "includes/gbuffer_unpack.glsl"

layout(rgba32f, set = 2, binding = 0) uniform readonly image2D noisyInputImage;
layout(rgba32f, set = 2, binding = 1) uniform readonly image2D lastFrameSuperSamplesImage;

layout(rgba32f, set = 2, binding = 2) uniform writeonly image2D outputImage;
layout(rgba32f, set = 2, binding = 3) uniform writeonly image2D historyLengthImage;
layout(rgba32f, set = 2, binding = 4) uniform readonly image2D lastFrameHistoryLengthImage;
layout(set = 2, binding = 5) uniform texture2D previousViewPos;

vec4 AdjustHDRColor(vec4 color)
{
    /*float luminance = dot(color.rgb, vec3(0.299, 0.587, 0.114));
    float luminanceWeight = 1.0 / (1.0 + luminance);
    return vec4(color.rgb, 1.0) * luminanceWeight;*/
    return vec4(color.rgb, 1.0);
}

void main() {
    const ivec2 coords = ivec2(gl_GlobalInvocationID);
    const ivec2 size = imageSize(outputImage);
    const vec2 textureDimensions = vec2(size);
    if(coords.x >= size.x
    || coords.y >= size.y) {
        return;
    }

    const vec2 uv = coords/vec2(size);

    GBuffer gbuffer = unpackGBuffer(uv);
    vec4 currentFrameColor = imageLoad(noisyInputImage, coords);

    vec3 viewSpacePosH = gbuffer.viewPosition;
    bool isSkybox = gbuffer.albedo.a <= 0.01f;

    if(isSkybox) {
        vec2 moments = vec2(luminance(currentFrameColor.rgb), 0.0);
        moments.y = moments.x * moments.x;

        imageStore(outputImage, coords, vec4(moments, 1.0f, 1.0f));
        imageStore(outputImage, coords, currentFrameColor);
        return;
    }

    vec4 viewSpacePos = vec4(viewSpacePosH.rgb, 1.0);
    vec4 hWorldSpacePos = cbo.inverseView * viewSpacePos;

    vec4 prevNDC = cbo.jitteredProjection * viewSpacePos;
    prevNDC.xyz /= prevNDC.w;

    vec2 reprojectedUV = uv + (gbuffer.motionVector.xy/2);//(prevNDC.xy + gbuffer.motionVector.xy) / 2.0 + 0.5;
    vec4 previousViewSpacePos = vec4(texture(sampler2D(previousViewPos, gNearestSampler), reprojectedUV).xyz, 1);
    vec4 hPreviousWorldSpacePos = previousFrameCBO.inverseView * previousViewSpacePos;

    bool reprojectionInBounds = reprojectedUV.x >= 0.0f && reprojectedUV.x < 1.0f && reprojectedUV.y >= 0.0f && reprojectedUV.y < 1.0f;
    float reprojectionFactor = reprojectionInBounds ? 1.0f : 0.0f;
    vec4 momentHistoryHistoryLength = imageLoad(lastFrameHistoryLengthImage, ivec2(reprojectedUV * textureDimensions + 0.5));
    if(reprojectionInBounds) {
        reprojectionFactor *= exp(-distance(hPreviousWorldSpacePos.xyz, hWorldSpacePos.xyz) / 0.01);
    }

    bool reprojected = false;
    if(reprojectionFactor > 0.1f) {
        reprojected = true;
    } else {
        momentHistoryHistoryLength = vec4(0.0);
    }

    vec4 outColor = vec4(0);

    #if 0 // does not really make sense for AO
    vec3 minColor = vec3(100000);
    vec3 maxColor = vec3(-100000);

    for(int x = -1; x <= 1; x++) {
        for(int y = -1; y <= 1; y++) {
            vec4 color = AdjustHDRColor(imageLoad(noisyInputImage, coords + ivec2(x, y)));
            minColor = min(minColor, color.rgb);
            maxColor = max(maxColor, color.rgb);
        }
    }
    vec4 previousFrameColor = AdjustHDRColor(imageLoad(lastFrameSuperSamplesImage, ivec2(reprojectedUV * textureDimensions + 0.5)));
    vec3 previousFrameColorClamped = clamp(previousFrameColor.rgb, minColor, maxColor);
    #elif 0
    vec4 previousFrameColor = AdjustHDRColor(imageLoad(lastFrameSuperSamplesImage, ivec2(reprojectedUV * textureDimensions + 0.5)));
    vec3 previousFrameColorClamped = previousFrameColor.rgb;
    #else
    vec3 minColor = vec3(100000);
    vec3 maxColor = vec3(-100000);

    for(int x = -5; x <= 5; x++) {
        for(int y = -5; y <= 5; y++) {
            vec4 color = AdjustHDRColor(imageLoad(noisyInputImage, coords + ivec2(x, y)));
            minColor = min(minColor, color.rgb);
            maxColor = max(maxColor, color.rgb);
        }
    }
    vec4 previousFrameColor = AdjustHDRColor(imageLoad(lastFrameSuperSamplesImage, ivec2(reprojectedUV * textureDimensions + 0.5)));
    vec3 previousFrameColorClamped = mix(previousFrameColor.rgb, clamp(previousFrameColor.rgb, minColor, maxColor), 0.125);
    #endif
    // TODO: downsample and/or variance?

    float historyLength = momentHistoryHistoryLength.b * (reprojected ? 1 : 0) + 1.0;
    float alpha = min(0.99, 1.0f - 1.0f / historyLength);
    const float currentWeight = (1-alpha);
    const float previousWeight = alpha;
    const float normalization = 1.0f / (currentWeight + previousWeight);

    if(isnan(previousFrameColor.x) || !reprojected)
    {
        outColor = currentFrameColor;
        historyLength = 0.0f;
    }
    else
    {
        vec3 outRGB = (previousFrameColorClamped.rgb * previousWeight + currentFrameColor.rgb * currentWeight) * normalization;
        outColor = vec4(outRGB, 1.0);
    }

    vec2 moments = vec2(luminance(currentFrameColor.rgb), 0.0);
    moments.y = moments.x * moments.x;

    vec2 outMoments = (momentHistoryHistoryLength.xy * previousWeight + moments * currentWeight) * normalization;

    imageStore(historyLengthImage, coords, vec4(outMoments, historyLength, 1.0f));
    imageStore(outputImage, coords, outColor);
}