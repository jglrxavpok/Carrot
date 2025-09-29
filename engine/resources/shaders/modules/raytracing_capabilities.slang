module raytracing_capabilities;

// Represents the capabilities of the engine in terms of raytracing.
// Used to abstract hardware support and rendering techniques
public interface IRaytracingCapabilities {
    public uint getLightSampleCount();
    public uint getAOSampleCount();

    public bool checkVisibility(float3 from, float3 to);
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
}