//
// Created by jglrxavpok on 23/04/2021.
//

#include "SoundListener.h"
#include "OpenAL.hpp"

void Carrot::SoundListener::updatePosition(glm::vec3 position) {
    alListener3f(AL_POSITION, position.x, position.y, position.z);
}

void Carrot::SoundListener::updateVelocity(glm::vec3 velocity) {
    alListener3f(AL_VELOCITY, velocity.x, velocity.y, velocity.z);
}
