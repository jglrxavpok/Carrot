//
// Created by jglrxavpok on 28/07/2021.
//

#pragma once
#include "engine/ecs/components/Component.h"
#include "engine/render/Model.h"

namespace Carrot::ECS {
    struct ModelComponent: public IdentifiableComponent<ModelComponent> {
        std::shared_ptr<Carrot::Model> model;
        glm::vec4 color = glm::vec4{1.0f};
        bool isTransparent = false;

        explicit ModelComponent(Entity entity): IdentifiableComponent<ModelComponent>(std::move(entity)) {}

        explicit ModelComponent(const rapidjson::Value& json, Entity entity);

        rapidjson::Value toJSON(rapidjson::Document& doc) const override;

        const char *const getName() const override {
            return "ModelComponent";
        }

        std::unique_ptr<Component> duplicate(const Entity& newOwner) const override {
            auto result = std::make_unique<ModelComponent>(newOwner);
            result->model = model;
            result->isTransparent = isTransparent;
            result->color = color;
            return result;
        }

        void drawInspectorInternals(const Render::Context& renderContext, bool& modified) override;

    private:
        static ModelComponent* inInspector;
    };
}

template<>
inline const char* Carrot::Identifiable<Carrot::ECS::ModelComponent>::getStringRepresentation() {
    return "ModelComponent";
}