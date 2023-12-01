//
// Created by jglrxavpok on 19/11/2023.
//

#pragma once

#include <cstdint>

namespace Carrot::Render {
    struct Meshlet {
        std::uint32_t vertexOffset = 0;
        std::uint32_t vertexCount = 0;
        std::uint32_t indexOffset = 0;
        std::uint32_t indexCount = 0;

        std::uint32_t lod = 0;
    };
}