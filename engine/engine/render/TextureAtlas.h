//
// Created by jglrxavpok on 05/08/2021.
//

#pragma once

#include <cstdint>
#include <vector>
#include "core/math/Rect2D.hpp"

namespace Carrot::Render {
    class TextureAtlas {
    public:
        enum class AddressingMode {
            ThrowOutOfBounds, // throw an exception when accessing an index outside of this atlas
            Repeat, // accesses the index modulo the range of this atlas (works for negative numbers)
            Clamp, // accessing an index outside of the atlas will clamp to min/max accepted values
        };

        explicit TextureAtlas(std::uint32_t xElements,
                              std::uint32_t yElements,
                              AddressingMode addressingMode = AddressingMode::ThrowOutOfBounds)
                              : xElements(xElements), yElements(yElements), addressingMode(addressingMode) {
            elementWidth = 1.0f / static_cast<float>(xElements);
            elementHeight = 1.0f / static_cast<float>(yElements);
        }

    public:
        [[nodiscard]] Carrot::Math::Rect2Df get(std::int32_t x, std::int32_t y) const;
        [[nodiscard]] Carrot::Math::Rect2Df operator[](std::pair<std::int32_t, std::int32_t> xy) const { return get(xy.first, xy.second); }
        [[nodiscard]] Carrot::Math::Rect2Df operator[](glm::ivec2 xy) const { return get(xy.x, xy.y); }

    public:
        /// Returns all indices available in this atlas, row-by-row, column by column (left to right, then top to bottom)
        [[nodiscard]] std::vector<glm::ivec2> getAllIndices() const;

        [[nodiscard]] std::vector<glm::ivec2> getRowIndices(std::int32_t row) const;
        [[nodiscard]] std::vector<glm::ivec2> getColumnIndices(std::int32_t column) const;


    private:
        std::uint32_t xElements; // # of elements on X axis
        std::uint32_t yElements; // # of elements on Y axis
        float elementWidth = 0.0f;
        float elementHeight = 0.0f;
        AddressingMode addressingMode;
    };
}
