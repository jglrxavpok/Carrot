//
// Created by jglrxavpok on 27/04/2021.
//

#include <glm/gtx/quaternion.hpp>

#include "Particles.h"
#include "core/utils/RNG.h"

#include "core/math/Constants.h"

Carrot::ParticleEmitter::ParticleEmitter(Carrot::ParticleSystem& system, u32 emitterID): system(system), emitterID(emitterID) {

}

bool Carrot::ParticleEmitter::isEmittingInWorldSpace() const {
    return emitInWorldSpace;
}

void Carrot::ParticleEmitter::setEmittingInWorldSpace(bool inWorldSpace) {
    if (emitInWorldSpace != inWorldSpace) {
        emitInWorldSpace = inWorldSpace;
        needEmitterTransformUpdate = true;
    }
}

void Carrot::ParticleEmitter::setPosition(const glm::vec3& newPosition) {
    if (this->position != newPosition) {
        this->position = newPosition;
        needEmitterTransformUpdate = true;
    }
}

void Carrot::ParticleEmitter::setRotation(const glm::quat& rotation) {
    if (this->rotation != rotation) {
        needEmitterTransformUpdate = true;
        this->rotation = rotation;
    }
}

const glm::quat& Carrot::ParticleEmitter::getRotation() const {
    return rotation;
}

void Carrot::ParticleEmitter::tick(double deltaTime) {
    if (needEmitterTransformUpdate) {
        needEmitterTransformUpdate = false;
        if (isEmittingInWorldSpace()) {
            emitterTransform = glm::identity<glm::mat4>();
        } else {
            emitterTransform = glm::translate(glm::mat4(1.0f), position) * glm::toMat4(rotation);
        }

        EmitterData& emitterData = system.getEmitterData(emitterID);
        emitterData.emitterTransform = emitterTransform;
    }
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
            particle->life = Math::Pi*2.0f;// * 10.0f;
        } else {
            particle->life = Math::Pi*1.0f;// * 10.0f;
        }
        particle->size = 1.0f;
        if (isEmittingInWorldSpace()) {
            particle->position = position;
        } else {
            particle->position = {};
        }
        particle->velocity = {
                Carrot::RNG::randomFloat(-0.5f, 0.5f),
                Carrot::RNG::randomFloat(-0.5f, 0.5f),
                Carrot::RNG::randomFloat(1.0f, 2.5f),
        };
        particle->emitterID = emitterID;

        spawnedParticles++;
    }
    time += deltaTime;
}
