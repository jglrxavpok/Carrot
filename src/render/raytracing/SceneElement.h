//
// Created by jglrxavpok on 01/03/2021.
//

#pragma once

namespace Carrot {
    struct SceneElement {
        alignas(16) std::uint32_t mappedIndex;
        alignas(16) glm::mat4 transform{1.0f};
    };
}