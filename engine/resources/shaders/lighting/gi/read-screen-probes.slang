import modules.gi;
import modules.camera;
import modules.base;

struct Data {
    RWTexture2D<float4> destination;
    ScreenProbes probes;
}
[[vk::binding(0,0)]] ParameterBlock<Data> data;
ParameterBlock<Camera> cameras;

struct PushConstants {
    uint frameCount;
    uint frameWidth;
    uint frameHeight;
}

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
    
    // bilinear interpolation
    const uint2 probePositionTopLeft = (coords.xy / ScreenProbeSize) * ScreenProbeSize;
    const uint2 probePositionTopRight = probePositionTopLeft + uint2(halfScreenProbesize, 0);
    const uint2 probePositionBottomLeft = probePositionTopLeft + uint2(0, halfScreenProbesize);
    const uint2 probePositionBottomRight = probePositionTopLeft + uint2(halfScreenProbesize, halfScreenProbesize);
    const float w11 = (probePositionTopRight.x*ScreenProbeSize - coords.x)*(probePositionTopRight.y*ScreenProbeSize - coords.y);
    const float w12 = (probePositionTopRight.x*ScreenProbeSize - coords.x)*(coords.y - probePositionBottomRight.y*ScreenProbeSize);
    const float w21 = (coords.x - probePositionTopLeft.x*ScreenProbeSize)*(probePositionTopRight.y*ScreenProbeSize - coords.y);
    const float w22 = (coords.x - probePositionTopLeft.x*ScreenProbeSize)*(coords.y - probePositionBottomRight.y*ScreenProbeSize);
    float3 average = float3(0.0);
    const float3 cameraPosition = (cameras.CurrentFrame().inverseView * float4(0,0,0,1)).xyz;
    average += data.probes.getFromScreenPos(probePositionBottomLeft, push.frameWidth).readRadianceFromCameraPos(cameraPosition) * w11;
    average += data.probes.getFromScreenPos(probePositionBottomRight, push.frameWidth).readRadianceFromCameraPos(cameraPosition) * w21;
    average += data.probes.getFromScreenPos(probePositionTopLeft, push.frameWidth).readRadianceFromCameraPos(cameraPosition) * w12;
    average += data.probes.getFromScreenPos(probePositionTopRight, push.frameWidth).readRadianceFromCameraPos(cameraPosition) * w22;

    const float denominator = w11 + w12 + w21 + w22;
    const float invDenominator = 1.0f / denominator;
    average *= invDenominator;
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