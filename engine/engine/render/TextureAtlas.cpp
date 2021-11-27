//
// Created by jglrxavpok on 05/08/2021.
//

#include "TextureAtlas.h"
#include <stdexcept>
#include "core/math/BasicFunctions.h"

namespace Carrot::Render {

    Carrot::Math::Rect2Df TextureAtlas::get(std::int32_t x, std::int32_t y) const {
        if(x < 0 || x >= xElements || y < 0 || y >= yElements) {
            switch (addressingMode) {
                case AddressingMode::ThrowOutOfBounds:
                    throw std::runtime_error("Out-of-bounds atlas access");

                case AddressingMode::Clamp:
                    x = std::min(std::max(0, x), static_cast<std::int32_t>(xElements));
                    y = std::min(std::max(0, y), static_cast<std::int32_t>(yElements));
                    break;

                case AddressingMode::Repeat:
                    x = Carrot::Math::imod(x, static_cast<std::int32_t>(xElements));
                    y = Carrot::Math::imod(y, static_cast<std::int32_t>(yElements));
                    break;
            }
        }
        float xf = static_cast<float>(x) / static_cast<float>(xElements);
        float yf = static_cast<float>(y) / static_cast<float>(yElements);
        return Carrot::Math::Rect2Df(xf, yf, xf + elementWidth, yf + elementHeight);
    }

    std::vector<glm::ivec2> TextureAtlas::getAllIndices() const {
        std::vector<glm::ivec2> result{xElements * yElements};
        for (std::int32_t y = 0; y < yElements; y++) {
            for (std::int32_t x = 0; x < xElements; x++) {
                result[x + y * xElements] = {x, y};
            }
        }
        return result;
    }

    std::vector<glm::ivec2> TextureAtlas::getColumnIndices(std::int32_t column) const {
        std::vector<glm::ivec2> result{yElements};
        for (std::int32_t y = 0; y < yElements; y++) {
            result[y] = {column, y};
        }
        return result;
    }

    std::vector<glm::ivec2> TextureAtlas::getRowIndices(std::int32_t row) const {
        std::vector<glm::ivec2> result{xElements};
        for (std::int32_t x = 0; x < xElements; x++) {
            result[x] = {x, row};
        }
        return result;
    }
}