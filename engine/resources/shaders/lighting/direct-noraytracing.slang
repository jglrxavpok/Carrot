import direct_base;
import modules.lighting;
import modules.gi;
import modules.raytracing_capabilities;

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uniform LightingPassUniforms push,
    uint3 coords: SV_DispatchThreadID
) {
    NullRaytracingCapability capability;
    computeDirectLighting(capability, coords.xy, push);
}