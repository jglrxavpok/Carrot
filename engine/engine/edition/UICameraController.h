//
// Created by jglrxavpok on 21/06/2026.
//

#pragma once

#include <rapidjson/document.h>

namespace Carrot {
    class Camera;
}

namespace Carrot::Edition {
    /// Camera controller that only allows for panning and scaling, and orthographic projection
    /// Used for 2D UI edition
    struct UICameraController {
        explicit UICameraController() = default;
        UICameraController(const UICameraController&) = default;
        UICameraController(UICameraController&&) = default;

        UICameraController& operator=(const UICameraController&) = default;
        UICameraController& operator=(UICameraController&&) = default;

        void deserialise(const rapidjson::Value& src);
        rapidjson::Value serialise(rapidjson::Document& dest) const;

        void applyTo(const glm::vec2& viewportSize, Carrot::Camera& camera) const;
    };
} // Carrot::Edition