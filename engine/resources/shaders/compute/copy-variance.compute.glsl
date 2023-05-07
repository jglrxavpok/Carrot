layout (local_size_x = 32) in;
layout (local_size_y = 32) in;

#include <includes/lighting_utils.glsl>

layout(rgba32f, set = 0, binding = 0) uniform readonly image2D inputImage;
layout(rgba32f, set = 0, binding = 1) uniform readonly image2D inputMomentsHistory;
layout(r32f, set = 0, binding = 2) uniform writeonly image2D outputImage;

void main() {
    // A-Trous filter
    const ivec2 coords = ivec2(gl_GlobalInvocationID);

    const ivec2 size = imageSize(outputImage);

    vec4 momentsHistoryData = imageLoad(inputMomentsHistory, coords);
    vec2 accumulatedMoments = momentsHistoryData.rg;
    float historyLength = momentsHistoryData.b;

    float variance = 0.0f;
    if(historyLength < 4) {
        // spatial estimation
        const int kernelSize = 3; // 7x7

        if(coords.x < kernelSize
        || coords.y < kernelSize
        || coords.x >= size.x - kernelSize
        || coords.y >= size.y - kernelSize) {
            return;
        }

        // spatial estimated
        vec2 moments = vec2(0.0);
        float totalWeight = 0.0f;
        for(int dx = -kernelSize; dx <= kernelSize; dx++) {
            for(int dy = -kernelSize; dy <= kernelSize; dy++) {
                vec4 color = imageLoad(inputImage, coords + ivec2(dx, dy));

                float weight = 1.0f;
                float lum = luminance(color.rgb);
                moments += vec2(lum, lum * lum) * weight;
                totalWeight += weight;
            }
        }

        moments /= totalWeight*totalWeight;
        variance = max(0.0, moments.g - moments.r * moments.r);
    } else {
        variance = max(0.0, accumulatedMoments.g - accumulatedMoments.r * accumulatedMoments.r);
    }

    imageStore(outputImage, coords, vec4(variance, 0, 0, 0));
}