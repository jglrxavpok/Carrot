//
// Created by jglrxavpok on 26/02/2021.
//
#include "TransformComponent.h"
#include <engine/ecs/World.h>
#include <core/utils/JSON.h>

namespace Carrot::ECS {
    glm::mat4 TransformComponent::toTransformMatrix() const {
        auto parent = getEntity().getParent();
        if(parent) {
            if(auto parentTransform = getEntity().getWorld().getComponent<TransformComponent>(parent.value())) {
                return transform.toTransformMatrix(&parentTransform->transform);
            }
        }
        return transform.toTransformMatrix();
    }

    void TransformComponent::drawInspectorInternals(const Render::Context& renderContext, bool& modified) {
        {
            float arr[] = { transform.position.x, transform.position.y, transform.position.z };
            if (ImGui::DragFloat3("Position", arr)) {
                transform.position = { arr[0], arr[1], arr[2] };
                modified = true;
            }
        }

        {
            float arr[] = { transform.scale.x, transform.scale.y, transform.scale.z };
            if (ImGui::DragFloat3("Scale", arr)) {
                transform.scale = { arr[0], arr[1], arr[2] };
                modified = true;
            }
        }

        {
            float arr[] = { transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w };
            if (ImGui::DragFloat4("Rotation", arr)) {
                transform.rotation = { arr[3], arr[0], arr[1], arr[2] };
                modified = true;
            }
        }
    }

    TransformComponent::TransformComponent(const rapidjson::Value& json, Entity entity): TransformComponent(std::move(entity)) {
        transform.position = JSON::read<3, float>(json["position"]);
        transform.scale = JSON::read<3, float>(json["scale"]);
        auto rotVec = JSON::read<4, float>(json["rotation"]);
        transform.rotation = glm::quat { rotVec.w, rotVec.x, rotVec.y, rotVec.z };
    };

    rapidjson::Value TransformComponent::toJSON(rapidjson::Document& doc) const {
        rapidjson::Value obj(rapidjson::kObjectType);
        obj.AddMember("position", JSON::write<3, float>(transform.position, doc), doc.GetAllocator());
        obj.AddMember("scale", JSON::write<3, float>(transform.scale, doc), doc.GetAllocator());
        glm::vec4 rotationVec { transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w };
        obj.AddMember("rotation", JSON::write<4, float>(rotationVec, doc), doc.GetAllocator());
        return obj;
    }

    glm::vec3 TransformComponent::computeFinalPosition() const {
        glm::vec4 wpos {0, 0, 0, 1.0};
        wpos = toTransformMatrix() * wpos;
        return { wpos.x, wpos.y, wpos.z };
    }
}