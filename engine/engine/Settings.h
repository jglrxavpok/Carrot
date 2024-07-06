//
// Created by jglrxavpok on 06/07/2024.
//

#pragma once

#include <cstdint>

namespace Carrot {
    struct Settings {
        /**
         * Should the game be locked at a given framerate?
         * If not 0, the engine will attempt to throttle itself to lock the framerate at the given value
         * Obviously, if the engine runs too slow in a given scene, it may not reach that framerate
         *
         * Expressed in Hz
         */
        std::uint32_t fpsLimit = 0;
    };
}