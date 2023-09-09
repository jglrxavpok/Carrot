//
// Created by jglrxavpok on 23/04/2021.
//

#pragma once

#include <glm/glm.hpp>

namespace Carrot::Audio {
    class SoundListener {
    private:
        explicit SoundListener() = default;

    public:
        static void updatePosition(const glm::vec3& position);
        static void updateVelocity(const glm::vec3& velocity);
    };
}
