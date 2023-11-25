//
// Created by jglrxavpok on 28/07/2021.
//

#pragma once
#include "engine/ecs/components/Component.h"
#include "engine/render/Model.h"
#include "engine/render/AsyncResource.hpp"
#include "engine/assets/AssetServer.h"
#include "engine/render/raytracing/ASBuilder.h"
#include <core/async/Locks.h>

namespace Carrot {
    class InstanceHandle; // Raytracing Top Level Acceleration Structure

    namespace Render {
        struct MeshletsInstance;
    }
}

namespace Carrot::ECS {
    struct ModelComponent: public IdentifiableComponent<ModelComponent> {
        AsyncModelResource asyncModel;
        std::shared_ptr<Render::ModelRenderer> modelRenderer;
        glm::vec4 color = glm::vec4{1.0f};
        bool isTransparent = false;
        std::shared_ptr<InstanceHandle> tlas = nullptr;
        std::unordered_map<Render::Viewport*, std::shared_ptr<Render::MeshletsInstance>> meshletsInstance;
        bool castsShadows = true;

        explicit ModelComponent(Entity entity): IdentifiableComponent<ModelComponent>(std::move(entity)) {}

        explicit ModelComponent(const rapidjson::Value& json, Entity entity);

        rapidjson::Value toJSON(rapidjson::Document& doc) const override;

        const char *const getName() const override {
            return "ModelComponent";
        }

        std::unique_ptr<Component> duplicate(const Entity& newOwner) const override {
            auto result = std::make_unique<ModelComponent>(newOwner);
            asyncModel.forceWait();
            result->asyncModel = std::move(AsyncModelResource(GetAssetServer().loadModelTask(Carrot::IO::VFS::Path { asyncModel->getOriginatingResource().getName() })));
            result->isTransparent = isTransparent;
            result->color = color;
            result->modelRenderer = modelRenderer;
            result->castsShadows = castsShadows;
            return result;
        }

        void setFile(const IO::VFS::Path& path);

    private:
        std::shared_ptr<Render::MeshletsInstance> loadMeshletsInstance(const Carrot::Render::Context& currentContext);
        void loadTLASIfPossible();
        void enableTLAS();
        void disableTLAS();

    private:
        Async::SpinLock tlasAccess;
        Async::SpinLock meshletsAccess;
        bool tlasIsWaitingForModel = true;

        friend class ModelRenderSystem;
    };
}

template<>
inline const char* Carrot::Identifiable<Carrot::ECS::ModelComponent>::getStringRepresentation() {
    return "ModelComponent";
}