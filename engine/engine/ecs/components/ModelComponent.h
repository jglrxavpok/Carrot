//
// Created by jglrxavpok on 28/07/2021.
//

#pragma once
#include "engine/ecs/components/Component.h"
#include "engine/render/Model.h"
#include "engine/render/AsyncResource.hpp"
#include "engine/render/raytracing/ASBuilder.h"

namespace Carrot {
    class InstanceHandle; // Raytracing Top Level Acceleration Structure
}

namespace Carrot::ECS {
    struct ModelComponent: public IdentifiableComponent<ModelComponent> {
        AsyncModelResource asyncModel;
        glm::vec4 color = glm::vec4{1.0f};
        bool isTransparent = false;
        std::shared_ptr<InstanceHandle> tlas = nullptr;

        explicit ModelComponent(Entity entity): IdentifiableComponent<ModelComponent>(std::move(entity)) {}

        explicit ModelComponent(const rapidjson::Value& json, Entity entity);

        rapidjson::Value toJSON(rapidjson::Document& doc) const override;

        const char *const getName() const override {
            return "ModelComponent";
        }

        std::unique_ptr<Component> duplicate(const Entity& newOwner) const override {
            auto result = std::make_unique<ModelComponent>(newOwner);
            asyncModel.forceWait();
            result->asyncModel = std::move(AsyncModelResource(GetRenderer().coloadModel(asyncModel->getOriginatingResource().getName())));
            result->isTransparent = isTransparent;
            result->color = color;
            return result;
        }

        void drawInspectorInternals(const Render::Context& renderContext, bool& modified) override;

        void setFile(const std::filesystem::path& path);

    private:
        static ModelComponent* inInspector;

    private:
        void loadTLASIfPossible();
        void enableTLAS();
        void disableTLAS();

    private:
        bool tlasIsWaitingForModel = true;

        friend class ModelRenderSystem;
    };
}

template<>
inline const char* Carrot::Identifiable<Carrot::ECS::ModelComponent>::getStringRepresentation() {
    return "ModelComponent";
}