module raytracing;

import modules.raytracing_capabilities;

namespace Raytracing {
    public struct Geometry {
        int dummy; // TODO
    }

    public struct Instance {
        public float4 instanceColor;
        public uint firstGeometryIndex;
    }

    public struct Data {
        public RaytracingAccelerationStructure tlas;
        public Texture2D noiseTexture;
        public StructuredBuffer<Geometry, ScalarDataLayout> geometries;
        public StructuredBuffer<Instance, ScalarDataLayout> instances;
    }

    public struct HardwareCapability: IRaytracingCapabilities {
        private Data data;

        public __init(Data dataIn) {
            data = dataIn;
        }

        public uint getLightSampleCount() {
            // TODO: more than one sample if light importance sampling
            return 1;
        }

        public uint getAOSampleCount() {
            return 4;
        }

        public bool checkVisibility(float3 from, float3 to) {
            // TODO: attempt screen space first?

            RayQuery<RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_CULL_NON_OPAQUE> query;
            RayDesc ray;
            float3 deltaPos = to-from;
            float distance = length(deltaPos);
            ray.Direction = deltaPos / distance;
            ray.Origin = from;
            ray.TMin = 0.0f;
            ray.TMax = distance;
            query.TraceRayInline(data.tlas, RAY_FLAG_NONE, 0xFFFFu, ray);
            query.Proceed();

            return query.CommittedStatus() != COMMITTED_TRIANGLE_HIT;
        }
    }
}
