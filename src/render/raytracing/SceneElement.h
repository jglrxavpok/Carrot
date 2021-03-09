//
// Created by jglrxavpok on 01/03/2021.
//

#pragma once

namespace Carrot {
    struct SceneElement {
        std::uint32_t mappedIndex;

        glm::mat4 transform{1.0f};

        /// inverse transpose of the transform matrix
        glm::mat4 transformIT{1.0f};
    };
}