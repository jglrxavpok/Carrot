layout(constant_id = 0) const uint MAX_PARTICLE_COUNT = 1000;

struct Particle {
    vec3 position;
    float life;
    vec3 velocity;
    float size;

    uint id;
};

layout(set = 1, binding = 0) buffer Particles {
    Particle particles[MAX_PARTICLE_COUNT];
};
