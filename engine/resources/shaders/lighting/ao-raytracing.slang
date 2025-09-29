import ao_base;

import modules.raytracing;
import modules.lighting;
[[vk::binding(0, 6)]] ParameterBlock<Raytracing::Data> rt;

[shader("compute")]
[numthreads(32, 32, 1)]
void main(uniform LightingPassUniforms push,
    uint3 coords: SV_DispatchThreadID
) {
    Raytracing::HardwareCapability capability = Raytracing::HardwareCapability(rt);
    computeAO(capability, coords.xy, push);
}