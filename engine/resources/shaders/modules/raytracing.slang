module raytracing;

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
}
