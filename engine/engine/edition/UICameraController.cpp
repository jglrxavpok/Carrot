//
// Created by jglrxavpok on 21/06/2026.
//

#include "UICameraController.h"

#include <engine/render/Camera.h>

namespace Carrot::Edition {

    void UICameraController::applyTo(const glm::vec2& viewportSize, Carrot::Camera& camera) const {
        glm::mat4 projection = glm::ortho(0.0f, 1.0f, 1.0f, 0.0f, -1.0f, 1.0f);
        glm::mat4 view = glm::mat4{1.0f};
        camera.setViewProjection(view, projection);
    }

    void UICameraController::deserialise(const rapidjson::Value& src) {
        // TODO
    }

    rapidjson::Value UICameraController::serialise(rapidjson::Document& dest) const {
        // TODO
        return rapidjson::Value{rapidjson::kObjectType};
    }
} // Carrot::Edition