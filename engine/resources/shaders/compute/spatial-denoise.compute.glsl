#include <includes/gbuffer.glsl>
#include <includes/lighting_utils.glsl>
#include <includes/gbuffer_input.glsl>

DEFINE_GBUFFER_INPUTS(0)

#include <includes/gbuffer_unpack.glsl>

const uint LOCAL_SIZE = 8;
layout (local_size_x = LOCAL_SIZE) in;
layout (local_size_y = LOCAL_SIZE) in;

layout(rgba32f, set = 1, binding = 0) uniform readonly image2D inputImage;
layout(rgba32f, set = 1, binding = 1) uniform readonly image2D varianceInput;

layout(rgba32f, set = 1, binding = 2) uniform writeonly image2D outputImage;
layout(rgba32f, set = 1, binding = 3) uniform writeonly image2D varianceOutput;
layout(set = 1, binding = 4) uniform texture2D firstBounceViewPositions;
layout(set = 1, binding = 5) uniform texture2D firstBounceViewNormals;

layout(push_constant) uniform Push {
    uint index;
} iterationData;

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
    e.viewPosition = texture(sampler2D(firstBounceViewPositions, gNearestSampler), uv).xyz;
    //e.viewPosition = g.viewPosition;
    e.normal = texture(sampler2D(firstBounceViewNormals, gNearestSampler), uv).xyz;
    //e.normal = g.viewTBN[2];
    e.entityID = g.entityID;
    return e;
}

shared ExtractedGBuffer sharedGBufferReads[LOCAL_SIZE][LOCAL_SIZE];
shared float sharedLuminanceMomentsReads[LOCAL_SIZE][LOCAL_SIZE];

ExtractedGBuffer readGBuffer(ivec2 coordsOffset/* offset from current pixel */) {

    ivec2 localCoords = ivec2(gl_LocalInvocationID.xy) + coordsOffset;
    if(localCoords.x >= 0 && localCoords.x < LOCAL_SIZE
    && localCoords.y >= 0 && localCoords.y < LOCAL_SIZE) {
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

float readVariance(ivec2 coordsOffset/* offset from current pixel */) {

    ivec2 localCoords = ivec2(gl_LocalInvocationID.xy) + coordsOffset;
    if(localCoords.x >= 0 && localCoords.x < LOCAL_SIZE
    && localCoords.y >= 0 && localCoords.y < LOCAL_SIZE) {
        return sharedLuminanceMomentsReads[localCoords.x][localCoords.y];
    }

    const ivec2 size = imageSize(varianceInput);
    const ivec2 coords = ivec2(gl_GlobalInvocationID) + coordsOffset;

    if(coords.x >= size.x
    || coords.y >= size.y
    || coords.x < 0
    || coords.y < 0
    ) {
        return 0.0f;
    }

    return imageLoad(varianceInput, coords).r;
}

float momentGaussianBlurSingleAxis(ivec2 coords, ivec2 direction) {
    float   color  = readVariance(-3 * direction) * ( 1.0/64.0);
            color += readVariance(-2 * direction) * ( 6.0/64.0);
            color += readVariance(-1 * direction) * (15.0/64.0);
            color += readVariance(+0 * direction) * (20.0/64.0);
            color += readVariance(+1 * direction) * (15.0/64.0);
            color += readVariance(+2 * direction) * ( 6.0/64.0);
            color += readVariance(+3 * direction) * ( 1.0/64.0);
    return color;
}

float momentGaussianBlur(ivec2 coords) {
    return max(0.0f, momentGaussianBlurSingleAxis(coords, ivec2(1, 0)) * momentGaussianBlurSingleAxis(coords, ivec2(0, 1)));
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
    return f64 * f64;
}

void main() {
    // A-Trous filter
    const ivec2 coords = ivec2(gl_GlobalInvocationID);

    // from SVGF
    // TODO: GP Direct

    const float sigmaPositions = 4.0f;
    const float sigmaLuminance = 2.0f;


    const int STEP_SIZE = 1 << iterationData.index;

    const ivec2 size = imageSize(outputImage);

    if(coords.x >= size.x
    || coords.y >= size.y) {
        sharedGBufferReads[gl_LocalInvocationID.x][gl_LocalInvocationID.y] = nullGBuffer();
        sharedLuminanceMomentsReads[gl_LocalInvocationID.x][gl_LocalInvocationID.y] = 0.0f;
        return;
    }
    vec4 finalPixel = imageLoad(inputImage, coords);

    const vec2 currentUV = coords/vec2(size);
    sharedGBufferReads[gl_LocalInvocationID.x][gl_LocalInvocationID.y] = extractUsefulInfo(unpackGBufferLight(currentUV), currentUV);
    sharedLuminanceMomentsReads[gl_LocalInvocationID.x][gl_LocalInvocationID.y] = imageLoad(varianceInput, ivec2(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y)).r;
    #define currentGBuffer (sharedGBufferReads[gl_LocalInvocationID.x][gl_LocalInvocationID.y])

    barrier();

    if(currentGBuffer.albedo.a <= 1.0f / 256.0f) {
        imageStore(outputImage, coords, finalPixel);
        imageStore(varianceOutput, coords, vec4(100000.0f));
        return;
    }

    const float baseLuminance = luminance(finalPixel.rgb);

    const vec3 currentPosition = currentGBuffer.viewPosition;
    float totalWeight = KERNEL_WEIGHTS[FILTER_RADIUS] * KERNEL_WEIGHTS[FILTER_RADIUS];
    finalPixel *= totalWeight;

    float variance = readVariance(ivec2(0)) * totalWeight * totalWeight;
    float varianceSum = totalWeight * totalWeight;

    const float invLuminanceWeightScale = 1.0f / (sigmaLuminance*sqrt(max(0.0, momentGaussianBlur(ivec2(0))))+10e-12);

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
            vec4 filterPixel = imageLoad(inputImage, filterCoord);
            if(filterGBuffer.albedo.a <= 1.0f/256.0f) {
                continue;
            }
            const float sameMeshWeight = currentGBuffer.entityID == filterGBuffer.entityID ? 1.0f : 0.0f;
            const float normalWeight = powNormals(max(0, dot(filterGBuffer.normal, currentGBuffer.normal)));

            const vec3 filterPosition = filterGBuffer.viewPosition;
            const vec3 dPosition = filterPosition - currentPosition;
            const float distanceSquared = dot(dPosition, dPosition);
            const float positionWeight = min(1, exp(-distanceSquared * sigmaPositions));

            const float filterLuminance = luminance(filterPixel.rgb);
            const float luminanceWeight = 1.0f;//min(1, exp(-abs(filterLuminance-baseLuminance) * invLuminanceWeightScale));

            const float weight = normalWeight * positionWeight * filterWeight * luminanceWeight;
            finalPixel += weight * filterPixel;
            totalWeight += weight;

            varianceSum += weight * weight;
            variance += readVariance(dCoord) * weight * weight;
        }
    }

    finalPixel /= totalWeight;
    imageStore(outputImage, coords, finalPixel);

    variance /= varianceSum;
    imageStore(varianceOutput, coords, vec4(variance, 0, 0, 0));
}