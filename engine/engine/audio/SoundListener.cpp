//
// Created by jglrxavpok on 23/04/2021.
//

#include "SoundListener.h"
#include "OpenAL.hpp"

void Carrot::Audio::SoundListener::updatePosition(const glm::vec3& position) {
    alListener3f(AL_POSITION, position.x, position.y, position.z);
}

void Carrot::Audio::SoundListener::updateVelocity(const glm::vec3& velocity) {
    alListener3f(AL_VELOCITY, velocity.x, velocity.y, velocity.z);
}
