//
// Created by jglrxavpok on 27/04/2021.
//

#include "Particles.h"
#include "engine/utils/RNG.h"

#include "engine/math/Constants.h"

Carrot::ParticleEmitter::ParticleEmitter(Carrot::ParticleSystem& system): system(system) {

}

void Carrot::ParticleEmitter::tick(double deltaTime) {
    rateError += deltaTime * rate;
    float remaining = fmod(rateError, 1.0f);
    uint64_t toSpawn = ceil(rateError-remaining);
    rateError = remaining;

    for (int i = 0; i < toSpawn; ++i) {
        auto* particle = system.getFreeParticle();
        if(!particle)
            break;
        assert(particle->life < 0.0f);
        particle->id = spawnedParticles;
        bool b = Carrot::RNG::randomFloat() > 0.5f;
        if(b) {
            particle->life = Math::Pi*2.0f * 10.0f;
        } else {
            particle->life = Math::Pi*1.0f * 10.0f;
        }
        particle->size = 1.0f;
        particle->position = position;
        particle->velocity = {
                Carrot::RNG::randomFloat(-0.5f, 0.5f),
                Carrot::RNG::randomFloat(-0.5f, 0.5f),
                Carrot::RNG::randomFloat(1.0f, 2.5f),
        };

        spawnedParticles++;
    }
    time += deltaTime;
}
