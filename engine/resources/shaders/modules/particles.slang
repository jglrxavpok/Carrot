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
    float dt;
    uint32_t particleCount;
}

public struct Particles {
    public StructuredBuffer<Particle, ScalarDataLayout> list;
    public StructuredBuffer<Stats, ScalarDataLayout> stats;
    public StructuredBuffer<Emitter, ScalarDataLayout> emitters;
    
}