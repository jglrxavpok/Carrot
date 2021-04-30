//
// Created by jglrxavpok on 27/04/2021.
//

#pragma once

#include <random>

namespace Carrot::RNG {
    inline float randomFloat(float min = 0.0f, float max = 1.0f) {
        static std::random_device rd;
        static std::mt19937 mt(rd());
        static std::uniform_real_distribution<double> distribution(0.0, 1.0);
        return distribution(mt) * (max-min) + min;
    }
}