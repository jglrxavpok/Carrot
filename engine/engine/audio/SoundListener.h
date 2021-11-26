//
// Created by jglrxavpok on 23/04/2021.
//

#pragma once

#include <glm/glm.hpp>

namespace Carrot {
    class SoundListener {
    private:
        explicit SoundListener() = default;

    public:
        void updatePosition(glm::vec3 position);
        void updateVelocity(glm::vec3 velocity);

        static SoundListener& instance() {
            static SoundListener listener;
            return listener;
        }
    };
}
