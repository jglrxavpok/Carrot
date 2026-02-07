import modules.base;
import modules.camera;
import modules.gbuffer;
import modules.gi;
import modules.compute_fullscreen;

struct Data {
    RWTexture2D<float4> destination;
    ScreenProbes probes;
}
[[vk::binding(0,0)]] ParameterBlock<Data> data;
ParameterBlock<GBufferInputs> gBufferInputs;
ParameterBlock<Camera> cameras;

struct PushConstants {
    uint frameCount;
    uint frameWidth;
    uint frameHeight;
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
    return f2;
}

float3 addProbeInfluence(const in PixelInfo pixelInfo, const uint2 probePosition, const uint frameWidth, const float3 cameraPosition, inout float weight) {
    if(pixelInfo.isSky) {
        return 0;
    }
    ScreenProbe probe = data.probes.getFromScreenPos(probePosition, frameWidth);
    const float normalWeight = powNormals(max(0, dot(probe.normal, pixelInfo.worldNormal)));

    const float sigmaPositions = 0.001f;
    const float3 dPosition = probe.worldPos - pixelInfo.worldPosition;
    const float distanceSquared = dot(dPosition, dPosition);
    const float positionWeight = min(1, exp(-distanceSquared * sigmaPositions));

    weight *= positionWeight * normalWeight;
    return probe.readRadianceFromCameraPos(cameraPosition) * weight;
}

static const float KERNEL_WEIGHTS[3] = {
    1.0f,
    3.5f,
    1.0f,
};

[shader("compute")]
[numthreads(32, 32, 1)]
void main(
    uniform PushConstants push,

    uint3 coords: SV_DispatchThreadID) {
    uint w, h;
    data.destination.GetDimensions(w, h);
    if(coords.x >= w || coords.y >= h) {
        return;
    }

    const uint halfScreenProbesize = ScreenProbeSize/2;
    if(coords.x < halfScreenProbesize)
    {
        coords.x = 0;
    }
    else
    {
        coords.x -= halfScreenProbesize;
    }
    if(coords.y < halfScreenProbesize)
    {
        coords.y = 0;
    }
    else
    {
        coords.y -= halfScreenProbesize;
    }

    const float2 uv = float2(coords.xy + 0.5) / float2(w, h);
    PixelInfo pixelInfo = PixelInfo(cameras.CurrentFrame(), gBufferInputs, uv);
    if(pixelInfo.isSky) {
        data.destination.Store(coords.xy, float4(1));
        return;
    }
    
    // bilinear interpolation
    const float3 cameraPosition = (cameras.CurrentFrame().inverseView * float4(0,0,0,1)).xyz;
    const uint2 probePositionCenter = (coords.xy / ScreenProbeSize) * ScreenProbeSize;
    float3 average = float3(0.0);
    float totalWeight = 0.0f;

    constexpr int Radius = 1;
    for(int dy = -Radius; dy <= Radius; dy++) {
        for(int dx = -Radius; dx <= Radius; dx++) {
            const uint2 probePosition = uint2(int2(probePositionCenter) + int2(dx, dy));
            float distanceWeight = 1.0/(1+distance(probePosition, coords.xy));
            float weight = KERNEL_WEIGHTS[dx+Radius] * KERNEL_WEIGHTS[dy+Radius] * distanceWeight;
            average += addProbeInfluence(pixelInfo, probePosition, push.frameWidth, cameraPosition, weight);
            totalWeight += weight;
        }
    }

    if(totalWeight > 0.01) {
        const float invDenominator = 1.0f / totalWeight;
        average *= invDenominator;
    } else {
        average = float3(0);
    }
    data.destination.Store(coords.xy, float4(average, 1));
}

[shader("compute")]
[numthreads(32, 32, 1)]
void debug(
    uniform PushConstants push,

    uint3 coords: SV_DispatchThreadID) {
    uint w, h;
    data.destination.GetDimensions(w, h);
    if(coords.x >= w || coords.y >= h) {
        return;
    }
    
    const float3 cameraPosition = (cameras.CurrentFrame().inverseView * float4(0,0,0,1)).xyz;
    const ScreenProbe probe = data.probes.getFromScreenPos(coords.xy, push.frameWidth);
    float3 pixel = probe.readRadianceFromCameraPos(cameraPosition);
    data.destination.Store(coords.xy, float4(pixel, 1));
}