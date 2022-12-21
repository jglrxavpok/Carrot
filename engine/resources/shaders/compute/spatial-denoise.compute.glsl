#include <includes/lighting_utils.glsl>
#include <includes/gbuffer_input.glsl>

DEFINE_GBUFFER_INPUTS(0)

#include <includes/gbuffer_unpack.glsl>

layout (local_size_x = 1, local_size_y = 1) in;

layout(rgba32f, set = 1, binding = 0) uniform readonly image2D inputImage;
layout(rgba32f, set = 1, binding = 1) uniform writeonly image2D outputImage;

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

    GBuffer currentGBuffer = unpackGBuffer(coords/vec2(size));
    float totalWeight = 1.0f;
    for(int dy = -FILTER_RADIUS; dy <= FILTER_RADIUS; dy++) {
        for(int dx = -FILTER_RADIUS; dx <= FILTER_RADIUS; dx++) {
            if(dx == 0 && dy == 0)
                continue;
            const ivec2 filterCoord = coords + ivec2(dx, dy) * STEP_SIZE;

            /*if(filterCoord.x < 0 || filterCoord.y < 0
            || filterCoord.x >= size.x || filterCoord.y >= size.y)
                continue;*/

            GBuffer filterGBuffer = unpackGBuffer(filterCoord / vec2(size));
            // TODO: use index for mesh on top of entity (for instance curtains of Sponza)
            float sameMeshWeight = currentGBuffer.entityID == filterGBuffer.entityID ? 1.0f : 0.0f;
            float weight = sameMeshWeight; // TODO
            finalPixel += weight * imageLoad(inputImage, filterCoord);
            totalWeight += weight;
        }
    }

    finalPixel /= totalWeight;
    imageStore(outputImage, coords, finalPixel);
}