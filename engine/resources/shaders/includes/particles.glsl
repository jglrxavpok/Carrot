layout(constant_id = 0) const uint MAX_PARTICLE_COUNT = 1000;

struct Particle {
    vec3 position;
    float life;
    vec3 velocity;
    float size;

    uint id;
};

layout(set = 0, binding = 1) buffer Particles {
    Particle particles[MAX_PARTICLE_COUNT];
};
