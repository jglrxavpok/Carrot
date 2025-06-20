//
// Created by jglrxavpok on 28/07/2021.
//

#pragma once
#include "engine/ecs/components/Component.h"
#include "engine/render/Model.h"
#include "engine/render/ModelRenderer.h"
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
        Render::ModelRendererStorage rendererStorage;

        explicit ModelComponent(Entity entity): IdentifiableComponent<ModelComponent>(std::move(entity)) {}

        explicit ModelComponent(const Carrot::DocumentElement& json, Entity entity);

        Carrot::DocumentElement serialise() const override;

        const char *const getName() const override {
            return "ModelComponent";
        }

        std::unique_ptr<Component> duplicate(const Entity& newOwner) const override;

        void setFile(const IO::VFS::Path& path);

    private:
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