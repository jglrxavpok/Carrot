// "simple" a-trous edge-stopping gaussian blur
interface IConfiguration {
    /// Converts the rgba input to the format supported for this configuration
    float4 swizzle(float4 rgba);

    /// Converts the given swizzled input to a RGBA vector
    float4 convertToFinalFormat(float4 swizzled);
}

extern struct Configuration : IConfiguration;

import modules.gbuffer;

ParameterBlock<GBufferInputs> gbufferInputs;

struct DenoiserIO {
    Texture2D<float4> input;
    RWTexture2D<float4> output;
}
ParameterBlock<DenoiserIO> io;

static const int FILTER_RADIUS = 2;
static const float KERNEL_WEIGHTS[FILTER_RADIUS*2+1] = {
    1.0f/16.0f,
    1.0f/4.0f,
    3.0f/8.0f,
    1.0f/4.0f,
    1.0f/16.0f,
};
static const uint LOCAL_SIZE_X = 16;
static const uint LOCAL_SIZE_Y = 8;

// used to workaround maxComputeSharedMemorySize
struct ExtractedGBuffer {
    float4 albedo;
    float3 viewPosition;
    float3 normal;
    uint4 entityID;

    __init() {
        albedo = float4(0);
        viewPosition = float3(0);
        normal = float3(1);
        entityID = uint4(0);
    }
};

ExtractedGBuffer nullGBuffer() {
    ExtractedGBuffer r;
    return r;
}

ExtractedGBuffer extractUsefulInfo(in GBuffer g) {
    ExtractedGBuffer e;
    e.albedo = g.albedo;
    e.viewPosition = g.viewPosition;
    e.normal = g.viewTBN[2];
    e.entityID = g.entityID;
    return e;
}

groupshared ExtractedGBuffer sharedGBufferReads[LOCAL_SIZE_X][LOCAL_SIZE_Y];

static uint outputWidth;
static uint outputHeight;

ExtractedGBuffer readGBuffer(uint2 globalCoords, uint2 localCoords, int2 coordsOffset/* offset from current pixel */) {
    int2 localCoords = localCoords + coordsOffset;
    if(localCoords.x >= 0 && localCoords.x < LOCAL_SIZE_X
    && localCoords.y >= 0 && localCoords.y < LOCAL_SIZE_Y) {
        return sharedGBufferReads[localCoords.x][localCoords.y];
    }

    const int2 coords = int2(globalCoords) + coordsOffset;

    if(coords.x >= outputWidth
    || coords.y >= outputHeight
    || coords.x < 0
    || coords.y < 0
    ) {
        return nullGBuffer();
    }

    const float2 filterUV = float2(coords) / float2(outputWidth, outputHeight);
    return extractUsefulInfo(gbufferInputs.read(filterUV));
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

void denoise(
    uint iterationIndex,
    uint3 coords,
    uint3 localCoords
) {
    Configuration config;
    const float sigmaPositions = 32.0f;
    const float sigmaLuminance = 2.0f;

    io.output.GetDimensions(outputWidth, outputHeight);

    if(coords.x >= outputWidth
    || coords.y >= outputHeight) {
        sharedGBufferReads[localCoords.x][localCoords.y] = nullGBuffer();
        return;
    }
    float4 finalPixel = config.swizzle(io.input.Load(coords));

    const float2 currentUV = float2(coords.xy)/float2(outputWidth, outputHeight);
    ExtractedGBuffer currentGBuffer = extractUsefulInfo(gbufferInputs.read(currentUV));
    sharedGBufferReads[localCoords.x][localCoords.y] = currentGBuffer;

    if(currentGBuffer.albedo.a <= 1.0f / 256.0f) {
        io.output.Store(coords.xy, config.convertToFinalFormat(finalPixel));
        return;
    }

    const float3 currentPosition = currentGBuffer.viewPosition;
    float totalWeight = KERNEL_WEIGHTS[FILTER_RADIUS] * KERNEL_WEIGHTS[FILTER_RADIUS];
    finalPixel *= totalWeight;

    const int STEP_SIZE = 1 << iterationIndex;

    bool firstIter = true;
    for(int dy = -FILTER_RADIUS; dy <= FILTER_RADIUS; dy++) {
        const float yKernelWeight = KERNEL_WEIGHTS[dy+FILTER_RADIUS];
        for(int dx = -FILTER_RADIUS; dx <= FILTER_RADIUS; dx++) {
            if(dx == 0 && dy == 0) {
                continue;
            }
            const int2 dCoord = int2(dx, dy) * STEP_SIZE;
            const int2 filterCoord = int2(coords.xy) + dCoord;

            if(filterCoord.x < 0 || filterCoord.y < 0
            || filterCoord.x >= outputWidth || filterCoord.y >= outputHeight) {
                continue;
            }

            const float filterWeight = KERNEL_WEIGHTS[dx+FILTER_RADIUS] * yKernelWeight;
            if(firstIter) {
                firstIter = false;
                GroupMemoryBarrierWithGroupSync(); // barrier put right before actually needing the data
            }
            const ExtractedGBuffer filterGBuffer = readGBuffer(coords.xy, localCoords.xy, dCoord);
            // TODO: use index for mesh on top of entity (for instance curtains of Sponza)
            // TODO: use groupshared for sampling input
            float4 filterPixel = config.swizzle(io.input.Load(int3(filterCoord, 0)));
            if(filterGBuffer.albedo.a <= 1.0f/256.0f) {
                continue;
            }
            const float sameMeshWeight = all(currentGBuffer.entityID == filterGBuffer.entityID) ? 1.0f : 0.0f;
            const float normalWeight = powNormals(max(0, dot(filterGBuffer.normal, currentGBuffer.normal)));

            const float3 filterPosition = filterGBuffer.viewPosition;
            const float3 dPosition = filterPosition - currentPosition;
            const float distanceSquared = dot(dPosition, dPosition);
            const float positionWeight = min(1, exp(-distanceSquared * sigmaPositions));

            const float weight = normalWeight * positionWeight * filterWeight;
            finalPixel += weight * filterPixel;
            totalWeight += weight;
        }
    }

    finalPixel /= totalWeight;
    io.output.Store(coords.xy, config.convertToFinalFormat(finalPixel));
}