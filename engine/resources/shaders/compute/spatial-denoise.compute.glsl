#include <includes/lighting_utils.glsl>
#include <includes/gbuffer_input.glsl>

DEFINE_GBUFFER_INPUTS(0)

#include <includes/gbuffer_unpack.glsl>

layout (local_size_x = 32) in;
layout (local_size_y = 32) in;

layout(rgba32f, set = 1, binding = 0) uniform readonly image2D inputImage;
layout(rgba32f, set = 1, binding = 1) uniform writeonly image2D outputImage;
layout(rgba32f, set = 1, binding = 2) uniform readonly image2D lastFrameMomentHistoryHistoryLength;

layout(push_constant) uniform Push {
    int index;
} iterationData;

const int FILTER_RADIUS = 2;
const float KERNEL_WEIGHTS[FILTER_RADIUS*2+1] = {
    1.0f/16.0f,
    1.0f/4.0f,
    3.0f/8.0f,
    1.0f/4.0f,
    1.0f/16.0f,
};

void main() {
    // A-Trous filter
    const ivec2 coords = ivec2(gl_GlobalInvocationID);

    // from SVGF
    const float sigmaNormals = 128.0f;
    const float sigmaPositions = 1.0f;
    const float sigmaLuminance = 1.0f;

    const int STEP_SIZE = 1 << iterationData.index;

    const ivec2 size = imageSize(outputImage);

    if(coords.x >= size.x
    || coords.y >= size.y) {
        return;
    }
    vec4 finalPixel = imageLoad(inputImage, coords);

    const vec2 currentUV = coords/vec2(size);
    const GBuffer currentGBuffer = unpackGBufferLight(currentUV);

    if(currentGBuffer.albedo.a <= 1.0f / 256.0f) {
        imageStore(outputImage, coords, finalPixel);
        return;
    }

    float baseLuminance = luminance(finalPixel.rgb);

    const vec3 currentPosition = currentGBuffer.viewPosition;
    float totalWeight = KERNEL_WEIGHTS[FILTER_RADIUS] * KERNEL_WEIGHTS[FILTER_RADIUS];
    finalPixel *= totalWeight;

    for(int dy = -FILTER_RADIUS; dy <= FILTER_RADIUS; dy++) {
        const float yKernelWeight = KERNEL_WEIGHTS[dy+FILTER_RADIUS];
        for(int dx = -FILTER_RADIUS; dx <= FILTER_RADIUS; dx++) {
            if(dx == 0 && dy == 0) {
                continue;
            }
            const ivec2 dCoord = ivec2(dx, dy) * STEP_SIZE;
            const ivec2 filterCoord = coords + dCoord;

            if(filterCoord.x < 0 || filterCoord.y < 0
            || filterCoord.x >= size.x || filterCoord.y >= size.y) {
                continue;
            }

            const float filterWeight = KERNEL_WEIGHTS[dx+FILTER_RADIUS] * yKernelWeight;
            const vec2 filterUV = filterCoord / vec2(size);
            const GBuffer filterGBuffer = unpackGBufferLight(filterUV);
            // TODO: use index for mesh on top of entity (for instance curtains of Sponza)
            vec4 filterPixel = imageLoad(inputImage, filterCoord);
            if(filterGBuffer.albedo.a <= 1.0f/256.0f) {
                continue;
            }
            const float sameMeshWeight = currentGBuffer.entityID == filterGBuffer.entityID ? 1.0f : 0.0f;
            const float normalWeight = pow(max(0, dot(filterGBuffer.viewTBN[2], currentGBuffer.viewTBN[2])), sigmaNormals);

            const vec3 filterPosition = filterGBuffer.viewPosition;
            const vec3 dPosition = filterPosition - currentPosition;
            const float distanceSquared = dot(dPosition, dPosition);
            const float positionWeight = min(1, exp(-distanceSquared/sigmaPositions));

            const float filterLuminance = luminance(filterPixel.rgb);
            const float luminanceWeight = exp(-abs(filterLuminance-baseLuminance)*sigmaLuminance);

            const float weight = sameMeshWeight * normalWeight * positionWeight * filterWeight * luminanceWeight;
            finalPixel += weight * filterPixel;
            totalWeight += weight;
        }
    }

    finalPixel /= totalWeight;
    imageStore(outputImage, coords, finalPixel);
}