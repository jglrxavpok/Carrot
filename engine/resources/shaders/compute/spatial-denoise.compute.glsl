#include <includes/lighting_utils.glsl>

layout (local_size_x = 1, local_size_y = 1) in;

layout(rgba32f, set = 0, binding = 0) uniform readonly image2D inputImage;
layout(rgba32f, set = 0, binding = 1) uniform writeonly image2D outputImage;

layout(push_constant) uniform Push {
    int index;
} iterationData;

void main() {
    // A-Trous filter
    const ivec2 coords = ivec2(gl_GlobalInvocationID);

    const int FILTER_RADIUS = 1;
    const int STEP_SIZE = 1 << iterationData.index;

    const ivec2 size = imageSize(inputImage);
    vec4 finalPixel = vec4(0.0);

    float totalWeight = 0.0f;
    for(int dy = -FILTER_RADIUS; dy <= FILTER_RADIUS; dy++) {
        for(int dx = -FILTER_RADIUS; dx <= FILTER_RADIUS; dx++) {
            const ivec2 filterCoord = coords + ivec2(dx, dy) * STEP_SIZE;

            /*if(filterCoord.x < 0 || filterCoord.y < 0
            || filterCoord.x >= size.x || filterCoord.y >= size.y)
                continue;*/

            float weight = 1.0f; // TODO
            finalPixel += weight * imageLoad(inputImage, filterCoord);
            totalWeight += weight;
        }
    }

    finalPixel /= totalWeight;
    imageStore(outputImage, coords, finalPixel);
}