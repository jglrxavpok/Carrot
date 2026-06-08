import modules.particles;
import modules.gbuffer;

import raster.draw_particles;

export ParticleRenderResult renderParticle(const in Particle particle, const in RenderInfo renderInfo, uint index, float dt) {
    ParticleRenderResult out;
    // MARKER TO REPLACE RENDER
    return out;
}

export ParticleUpdateResult updateParticle(const in Particle particle, uint index, float dt) {
    ParticleUpdateResult out;
    // MARKER TO REPLACE UPDATE
    return out;
}

[shader("vertex")]
ParticleVSOutput vertex(uint vertexID: SV_VertexID) {
    return vertexTemplate(vertexID);
}

[shader("pixel")]
EncodedGBuffer pixel(in ParticleVSOutput input) {
    return pixelTemplate(input);
}

[numthreads(1024, 1, 1)]
[shader("compute")]
void updateParticle(uint particleIndex: SV_DispatchThreadID) {
    updateTemplate(particleIndex);
}