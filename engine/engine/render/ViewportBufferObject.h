//
// Created by jglrxavpok on 27/11/2020.
//

#pragma once

#include <cstdint>

namespace Carrot {

    namespace Render {
        struct Context;
    }

    /// UBO used by the engine to send viewport information to shaders
    struct ViewportBufferObject {
        std::uint32_t frameCount = 0;
        std::uint32_t frameWidth = 1;
        std::uint32_t frameHeight = 1;
        bool hasTLAS = false;

        void update(const Render::Context& renderContext);
    };
}
