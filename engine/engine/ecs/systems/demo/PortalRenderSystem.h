//
// Created by jglrxavpok on 23/09/2026.
//

#pragma once
#include <engine/ecs/components/TransformComponent.h>
#include <engine/ecs/components/demo/PortalComponent.h>
#include <engine/ecs/systems/System.h>

namespace Carrot::ECS {
    class PortalRenderSystem: public RenderSystem<TransformComponent, PortalComponent>, public Identifiable<PortalRenderSystem> {
    public:
        Carrot::ECS::Entity entryPortal;
        Carrot::ECS::Entity exitPortal;

        explicit PortalRenderSystem(World& world)
            : RenderSystem<TransformComponent, PortalComponent>(world) {}

        PortalRenderSystem(const Carrot::DocumentElement& doc, World& world)
            : PortalRenderSystem(world) {}

    public:
        void onFrame(const Carrot::Render::Context& renderContext) override;
        void setupCamera(Carrot::Render::Context renderContext) override;

    protected:
        void onEntityAdded(Entity& entity) override;

    public:
        inline static const char *getStringRepresentation() {
            return "PortalRenderSystem";
        }

        virtual const char *getName() const override {
            return getStringRepresentation();
        }

        virtual std::unique_ptr<Carrot::ECS::System> duplicate(Carrot::ECS::World& newOwner) const override;

    private:
    };

} // Carrot::ECS

template<>
inline const char *Carrot::Identifiable<Carrot::ECS::PortalRenderSystem>::getStringRepresentation() {
    return Carrot::ECS::PortalRenderSystem::getStringRepresentation();
}