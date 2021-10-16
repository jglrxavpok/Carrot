//
// Created by jglrxavpok on 26/02/2021.
//
#include "Transform.h"
#include <engine/ecs/World.h>
#include <engine/utils/JSON.h>

namespace Carrot::ECS {
    glm::mat4 Transform::toTransformMatrix() const {
        auto modelRotation = glm::toMat4(rotation);
        auto matrix = glm::translate(glm::mat4(1.0f), position) * modelRotation * glm::scale(glm::mat4(1.0f), scale);
        auto parent = getEntity().getParent();
        if(parent) {
            if(auto parentTransform = getEntity().getWorld().getComponent<Transform>(parent.value())) {
                return parentTransform->toTransformMatrix() * matrix;
            }
        }
        return matrix;
    }

    void Transform::drawInspectorInternals(const Render::Context& renderContext, bool& modified) {
        {
            float arr[] = { position.x, position.y, position.z };
            if (ImGui::DragFloat3("Position", arr)) {
                position = { arr[0], arr[1], arr[2] };
                modified = true;
            }
        }

        {
            float arr[] = { scale.x, scale.y, scale.z };
            if (ImGui::DragFloat3("Scale", arr)) {
                scale = { arr[0], arr[1], arr[2] };
                modified = true;
            }
        }

        {
            float arr[] = { rotation.x, rotation.y, rotation.z, rotation.w };
            if (ImGui::DragFloat4("Rotation", arr)) {
                rotation = { arr[3], arr[0], arr[1], arr[2] };
                modified = true;
            }
        }
    }

    Transform::Transform(const rapidjson::Value& json, Entity entity): Transform(std::move(entity)) {
        position = JSON::read<3, float>(json["position"]);
        scale = JSON::read<3, float>(json["scale"]);
        auto rotVec = JSON::read<4, float>(json["rotation"]);
        rotation = glm::quat { rotVec.w, rotVec.x, rotVec.y, rotVec.z };
    };

    rapidjson::Value Transform::toJSON(rapidjson::Document& doc) const {
        rapidjson::Value obj(rapidjson::kObjectType);
        obj.AddMember("position", JSON::write<3, float>(position, doc), doc.GetAllocator());
        obj.AddMember("scale", JSON::write<3, float>(scale, doc), doc.GetAllocator());
        glm::vec4 rotationVec { rotation.x, rotation.y, rotation.z, rotation.w };
        obj.AddMember("rotation", JSON::write<4, float>(rotationVec, doc), doc.GetAllocator());
        return obj;
    }
}