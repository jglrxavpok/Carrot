/**
* Firefly rejection:
*  Estimates the color variance of each pixel and reduce the value of the pixel if it deviates too much from its neighbors
*  This is done to avoid noise dancing after blur
*/

layout (local_size_x = 32) in;
layout (local_size_y = 32) in;

#include <includes/lighting_utils.glsl>

layout(rgba32f, set = 0, binding = 0) uniform readonly image2D inputImage;
layout(rgba32f, set = 0, binding = 1) uniform writeonly image2D outputImage;

void main() {
    // A-Trous filter
    const ivec2 coords = ivec2(gl_GlobalInvocationID);

    const ivec2 size = imageSize(outputImage);

    // spatial estimation
    const int kernelSize = 3; // 7x7

    if(coords.x < kernelSize
    || coords.y < kernelSize
    || coords.x >= size.x - kernelSize
    || coords.y >= size.y - kernelSize) {
        return;
    }

    // TODO: share reads inside group
    vec3 radiance = imageLoad(inputImage, coords).rgb;

    // estimate color variance
    vec3 firstMoment = vec3(0.0);
    vec3 secondMoment = vec3(0.0);
    float totalWeight = 0.0f;
    for(int dx = -kernelSize; dx <= kernelSize; dx++) {
        for(int dy = -kernelSize; dy <= kernelSize; dy++) {
            vec4 color = imageLoad(inputImage, coords + ivec2(dx, dy));

            if(isnan(color.x) || isnan(color.y) || isnan(color.z))
            {
                color = vec4(0.0);
            }

            float weight = 1.0f;
            firstMoment += (color.rgb) * weight;
            secondMoment += (color.rgb * color.rgb) * weight * weight;
            totalWeight += weight;
        }
    }

    firstMoment /= totalWeight;
    secondMoment /= totalWeight * totalWeight;

    vec3 variance = max(vec3(0.0), secondMoment - firstMoment * firstMoment);

    // based on https://alain.xyz/blog/ray-tracing-denoising#filtering which is based on Ray Tracing Gems Chapter 25
    vec3 standardDeviation = sqrt(variance);
    vec3 highThreshold = 0.1 + firstMoment/* average */ + standardDeviation * 8.0;
    vec3 overflow = max(vec3(0.0), radiance - highThreshold);
    radiance = radiance - overflow;

    imageStore(outputImage, coords, vec4(radiance, 1.0));
}