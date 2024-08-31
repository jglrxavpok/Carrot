// "simple" a-trous edge-stopping gaussian blur
const uint LOCAL_SIZE_X = 16;
const uint LOCAL_SIZE_Y = 8;
layout (local_size_x = LOCAL_SIZE_X) in;
layout (local_size_y = LOCAL_SIZE_Y) in;

#include <includes/gbuffer.glsl>
#include <includes/lighting_utils.glsl>
#include <includes/gbuffer_input.glsl>

DEFINE_GBUFFER_INPUTS(0)

#include <includes/gbuffer_unpack.glsl>

layout(r8, set = 1, binding = 0) uniform readonly image2D inputImage;
layout(r8, set = 1, binding = 1) uniform writeonly image2D outputImage;

layout(push_constant) uniform PushConstant {
    uint iterationIndex;
} push;

const int FILTER_RADIUS = 2;
const float KERNEL_WEIGHTS[FILTER_RADIUS*2+1] = {
    1.0f/16.0f,
    1.0f/4.0f,
    3.0f/8.0f,
    1.0f/4.0f,
    1.0f/16.0f,
};

// used to workaround maxComputeSharedMemorySize
struct ExtractedGBuffer {
    vec4 albedo;
    vec3 viewPosition;
    vec3 normal;
    uvec4 entityID;
};

ExtractedGBuffer nullGBuffer() {
    ExtractedGBuffer r;
    r.albedo = vec4(0);
    r.viewPosition = vec3(0);
    r.normal = vec3(1);
    r.entityID = uvec4(0);
    return r;
}

ExtractedGBuffer extractUsefulInfo(in GBuffer g, vec2 uv) {
    ExtractedGBuffer e;
    e.albedo = g.albedo;
    e.viewPosition = g.viewPosition;
    e.normal = g.viewTBN[2];
    e.entityID = g.entityID;
    return e;
}

shared ExtractedGBuffer sharedGBufferReads[LOCAL_SIZE_X][LOCAL_SIZE_Y];

ExtractedGBuffer readGBuffer(ivec2 coordsOffset/* offset from current pixel */) {

    ivec2 localCoords = ivec2(gl_LocalInvocationID.xy) + coordsOffset;
    if(localCoords.x >= 0 && localCoords.x < LOCAL_SIZE_X
    && localCoords.y >= 0 && localCoords.y < LOCAL_SIZE_Y) {
        return sharedGBufferReads[localCoords.x][localCoords.y];
    }

    const ivec2 size = imageSize(outputImage);
    const ivec2 coords = ivec2(gl_GlobalInvocationID) + coordsOffset;

    if(coords.x >= size.x
    || coords.y >= size.y
    || coords.x < 0
    || coords.y < 0
    ) {
        return nullGBuffer();
    }

    const vec2 filterUV = vec2(coords) / vec2(size);
    return extractUsefulInfo(unpackGBuffer(filterUV), filterUV);
}

float powNormals(float f) {
    // sigma for normals is 128 with this function
    // pow is slow and log(0) is undefined in GLSL so no exp(log(a) * b)

    const float f2 = f * f;
    const float f4 = f2 * f2;
    const float f8 = f4 * f4;
    const float f16 = f8 * f8;
    const float f32 = f16 * f16;
    const float f64 = f32 * f32;
    return f32;
}

void main() {
    const ivec2 coords = ivec2(gl_GlobalInvocationID);

    const float sigmaPositions = 256.0f;
    const float sigmaLuminance = 2.0f;

    const ivec2 size = imageSize(outputImage);

    if(coords.x >= size.x
    || coords.y >= size.y) {
        sharedGBufferReads[gl_LocalInvocationID.x][gl_LocalInvocationID.y] = nullGBuffer();
        return;
    }
    float finalPixel = imageLoad(inputImage, coords).r;

    const vec2 currentUV = coords/vec2(size);
    sharedGBufferReads[gl_LocalInvocationID.x][gl_LocalInvocationID.y] = extractUsefulInfo(unpackGBufferLight(currentUV), currentUV);
    #define currentGBuffer (sharedGBufferReads[gl_LocalInvocationID.x][gl_LocalInvocationID.y])

    barrier();

    if(currentGBuffer.albedo.a <= 1.0f / 256.0f) {
        imageStore(outputImage, coords, finalPixel.rrrr);
        return;
    }

    const vec3 currentPosition = currentGBuffer.viewPosition;
    float totalWeight = KERNEL_WEIGHTS[FILTER_RADIUS] * KERNEL_WEIGHTS[FILTER_RADIUS];
    finalPixel *= totalWeight;

    const int STEP_SIZE = 1 << push.iterationIndex;

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
            const ExtractedGBuffer filterGBuffer = readGBuffer(dCoord);
            // TODO: use index for mesh on top of entity (for instance curtains of Sponza)
            float filterPixel = imageLoad(inputImage, filterCoord).r;
            if(filterGBuffer.albedo.a <= 1.0f/256.0f) {
                continue;
            }
            const float sameMeshWeight = currentGBuffer.entityID == filterGBuffer.entityID ? 1.0f : 0.0f;
            const float normalWeight = powNormals(max(0, dot(filterGBuffer.normal, currentGBuffer.normal)));

            const vec3 filterPosition = filterGBuffer.viewPosition;
            const vec3 dPosition = filterPosition - currentPosition;
            const float distanceSquared = dot(dPosition, dPosition);
            const float positionWeight = min(1, exp(-distanceSquared * sigmaPositions));

            const float weight = normalWeight * positionWeight * filterWeight;
            finalPixel += weight * filterPixel;
            totalWeight += weight;
        }
    }

    finalPixel /= totalWeight;
    imageStore(outputImage, coords, finalPixel.rrrr);
}