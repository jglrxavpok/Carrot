//
// Created by jglrxavpok on 28/07/2021.
//

#pragma once
#include "engine/ecs/components/Component.h"
#include "engine/render/Model.h"
#include "engine/render/ModelRenderer.h"
#include "engine/render/AsyncResource.hpp"
#include "engine/assets/AssetServer.h"
#include "engine/render/raytracing/RaytracingScene.h"
#include <core/async/Locks.h>

#include "ComponentReflection.h"

namespace Carrot {
    class InstanceHandle; // Raytracing Top Level Acceleration Structure
}

namespace Carrot::ECS {
    template<>
    struct ReflectedSerialisation<std::shared_ptr<Carrot::Render::ModelRenderer>> {
        static void deserialiseElement(ECS::Component& component, std::shared_ptr<Carrot::Render::ModelRenderer>& out, const Carrot::DocumentElement& doc);
        static Carrot::DocumentElement serialiseElement(const ECS::Component& component, const std::shared_ptr<Carrot::Render::ModelRenderer>& input);
    };

    struct ModelComponent: public ReflectionComponent<ModelComponent> {
        using ReflectionComponent::ReflectionComponent;

        FIELD(AsyncModelResource, modelResource, "Model", {});
        FIELD(std::shared_ptr<Render::ModelRenderer>, modelRenderer, "ModelRendererOverrides", {});
        FIELD(glm::vec4, color, "Color", glm::vec4{1.0f});
        FIELD(bool, isTransparent, "Transparent", false);
        Render::ModelRendererStorage rendererStorage;
        PROPERTY(bool, castsShadows, "CastsShadows", true,
            [](ModelComponent& self) { return self.rendererStorage.castsShadows; },
            [](ModelComponent& self, bool&& newValue) { self.rendererStorage.castsShadows = newValue; }
        );

        const char *const getName() const override {
            return "ModelComponent";
        }

        std::unique_ptr<Component> duplicate(const Entity& newOwner) const override;

        void setFile(const IO::VFS::Path& path);

        static void applyRewriteRules(Carrot::DocumentElement& doc);

    private:
        void enableTLAS();
        void disableTLAS();

    private:
        Async::SpinLock meshletsAccess;

        friend class ModelRenderSystem;
    };
}

template<>
inline const char* Carrot::Identifiable<Carrot::ECS::ModelComponent>::getStringRepresentation() {
    return "ModelComponent";
}