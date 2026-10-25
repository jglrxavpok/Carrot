//
// Created by jglrxavpok on 23/09/2026.
//

#pragma once
#include <engine/ecs/components/ModelComponent.h>
#include <engine/ecs/components/TransformComponent.h>
#include <engine/ecs/components/demo/PortalComponent.h>
#include <engine/ecs/systems/System.h>
#include <engine/render/Composer.h>

namespace Carrot::ECS {
    class PortalRenderSystem: public RenderSystem<TransformComponent, PortalComponent, ModelComponent>, public Identifiable<PortalRenderSystem> {
    public:
        explicit PortalRenderSystem(World& world)
            : RenderSystem<TransformComponent, PortalComponent, ModelComponent>(world) {}

        PortalRenderSystem(const Carrot::DocumentElement& doc, World& world)
            : PortalRenderSystem(world) {}

    public:
        void beginFrame(const Carrot::Render::Context& mainRenderContext) override;
        void onFrame(const Carrot::Render::Context& renderContext) override;
        void setupCamera(Carrot::Render::Context renderContext) override;

        void reload() override;
        void unload() override;

    protected:
        void onEntityAdded(Entity& entity) override;
        void onEntityRemoved(Entity& entity) override;

    public:
        inline static const char *getStringRepresentation() {
            return "PortalRenderSystem";
        }

        virtual const char *getName() const override {
            return getStringRepresentation();
        }

        virtual std::unique_ptr<Carrot::ECS::System> duplicate(Carrot::ECS::World& newOwner) const override;

    private:
        void doViewportRegeneration();

        struct ViewportInfo {
            Render::StencilSettings config;
            Entity entryPortal;
            i32 recursionIndex = -1;
            Carrot::Identifier previousViewportID; // used to inherit depth stencil
        };
        std::unordered_map<Carrot::Identifier, ViewportInfo> portalViewportInfo;
        bool regenerateViewports = true;
    };

} // Carrot::ECS

template<>
inline const char *Carrot::Identifiable<Carrot::ECS::PortalRenderSystem>::getStringRepresentation() {
    return Carrot::ECS::PortalRenderSystem::getStringRepresentation();
}