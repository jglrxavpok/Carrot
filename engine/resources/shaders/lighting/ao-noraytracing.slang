import ao_base;

import modules.raytracing_capabilities;
import modules.lighting;

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uniform LightingPassUniforms push,
    uint3 coords: SV_DispatchThreadID
) {
    NullRaytracingCapability capability;
    computeAO(capability, coords.xy, push);
}