import direct_base;
import modules.lighting;
import modules.raytracing;
import modules.gi;

[[vk::binding(0, 6)]] ParameterBlock<Raytracing::Data> rt;
[[vk::binding(0, 7)]] ParameterBlock<HashGrid::Block> gi;

export struct Configuration : IConfiguration {
    uint getSampleCount() {
        // TODO: more than one sample if light importance sampling
        return 1;
    }

    bool checkVisibility(float3 from, float3 to) {
        // TODO: attempt screen space first?

        RayQuery<RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_CULL_NON_OPAQUE> query;
        RayDesc ray;
        float3 deltaPos = to-from;
        float distance = length(deltaPos);
        ray.Direction = deltaPos / distance;
        ray.Origin = from;
        ray.TMin = 0.001f;
        ray.TMax = distance;
        query.TraceRayInline(rt.tlas, RAY_FLAG_NONE, 0xFFFFu, ray);
        query.Proceed();

        return query.CommittedStatus() != COMMITTED_TRIANGLE_HIT;
    }
}

[shader("compute")]
[numthreads(32, 32, 1)]
void main(uniform LightingPassUniforms push,
    uint3 coords: SV_DispatchThreadID
) {
    computeDirectLighting(coords.xy, push);
}