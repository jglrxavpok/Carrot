import direct_base;
import modules.lighting;
import modules.raytracing;
import modules.gi;

[[vk::binding(0, 6)]] ParameterBlock<Raytracing::Data> rt;

[shader("compute")]
[numthreads(32, 32, 1)]
void main(uniform LightingPassUniforms push,
    uint3 coords: SV_DispatchThreadID
) {
    Raytracing::HardwareCapability capability = Raytracing::HardwareCapability(rt);
    computeDirectLighting(capability, coords.xy, push);
}