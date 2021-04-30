#version 450

#include "../particles.glsl"

// particle index inside buffer
layout (local_size_x = 1024) in;

layout(set = 0, binding = 0) buffer ParticleStatistics {
    float deltaTime;
    uint totalCount;
} particleStats;

void main() {
    uint particleIndex = gl_GlobalInvocationID.x;

    if(particleIndex >= particleStats.totalCount) return;

    #define particle particles[particleIndex]

    float dt = particleStats.deltaTime;
    particle.position += particle.velocity * dt;

    float g = 2.0f;
    particle.velocity.z -= dt * g;
    particle.life -= dt;
    particle.size = particle.life*0.1;
}