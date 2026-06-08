import base;

public struct Particle {
    public float3 position;
    public float life;
    public float3 velocity;
    public float size;
    public uint32_t id;
    public uint32_t emitterID;
}

public struct Emitter {
    // transform to apply the particles drawn by this emitter, used to choose between world and local space emissions
    public mat4 transform;
}

public struct Stats {
    public float dt;
    public uint32_t particleCount;
}

public struct Particles {
    public RWStructuredBuffer<Particle, ScalarDataLayout> list;
    public StructuredBuffer<Stats, ScalarDataLayout> stats;
    public StructuredBuffer<Emitter, ScalarDataLayout> emitters;

    public ConstantBuffer<Texture2D[]> textures;
    public SamplerState linearSampler;
    public SamplerState nearestSampler;
}

public struct ParticleVSOutput {
    public float4 hPos : SV_Position;
    public nointerpolation uint32_t particleIndex;
    public float3 viewPosition;
    public float2 offsetFromCenter;
    public float2 uv;
}
