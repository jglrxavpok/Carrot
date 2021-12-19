//
// Created by jglrxavpok on 13/12/2021.
//

#pragma once

namespace Carrot {
    /// Capabilities of the engine when running. This depends on the hardware configuration, the OS and the graphics driver.
    struct Capabilities {
        // ---- Rendering capabilities ----
        bool supportsRaytracing = false;

        // ---- Misc. ----
        std::size_t hardwareParallelism = 1;
    };
}