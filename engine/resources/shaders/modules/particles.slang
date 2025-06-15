import base;

layout(constant_id = 0)
const uint32_t MaxParticlesCount = 1000;

public struct Particle {
    public float3 position;
    public float life;
    public float3 velocity;
    public float size;
    public uint32_t id;
}

public struct Particles {
    public StructuredBuffer<Particle> list;
}