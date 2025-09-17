//
// Created by jglrxavpok on 06/07/2024.
//

#pragma once

#include <cstdint>

namespace Carrot {
    struct GraphicsSettings {
        enum class ToneMappingOption: u8 {
            None,
            Reinhard,
            Aces
        } toneMapping = ToneMappingOption::Aces;
    };

    struct Settings {
        Settings(int argc, char** argv);

        /**
         * Should the game be locked at a given framerate?
         * If not 0, the engine will attempt to throttle itself to lock the framerate at the given value
         * Obviously, if the engine runs too slow in a given scene, it may not reach that framerate
         *
         * Expressed in Hz
         */
        std::uint32_t fpsLimit = 0;

        /**
         * Enable the use of Live++ hot reloading
         */
        bool useCppHotReloading = true;

        /**
         * Enable the use of Aftermath (crash reporting on Nvidia GPUs)
         */
        bool useAftermath = true;

        /**
         * Force the engine to use a single Vulkan queue. Slow and maybe unstable, only for debug
         */
        bool singleQueue = false;

        GraphicsSettings graphicsSettings;
    };
}