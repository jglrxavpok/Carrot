module raytracing_capabilities;
import modules.materials;

public struct SurfaceIntersection {
    public float3 worldPosition;
    public float3 worldNormal;
    public float3 worldTangent;

    public MaterialOutput computedMaterial;
}

// Represents the capabilities of the engine in terms of raytracing.
// Used to abstract hardware support and rendering techniques
public interface IRaytracingCapabilities {
    public uint getLightSampleCount();
    public uint getAOSampleCount();

    public bool checkVisibility(float3 from, float3 to);

    public Optional<SurfaceIntersection> raytrace(in MaterialSystem materials, Optional<float2> screenSpaceUV, float3 from, float3 direction, float maxDistance);
}

public struct NullRaytracingCapability: IRaytracingCapabilities {
    public uint getLightSampleCount() {
        return 1;
    }

    public uint getAOSampleCount() {
        return 1;
    }

    public bool checkVisibility(float3 from, float3 to) {
        return true;
    }

    public Optional<SurfaceIntersection> raytrace(in MaterialSystem materials, Optional<float2> screenSpaceUV, float3 from, float3 direction, float maxDistance) {
        // TODO: Screen space reflections?
        return none;
    }
}