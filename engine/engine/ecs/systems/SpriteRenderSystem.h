//
// Created by jglrxavpok on 28/07/2021.
//

#pragma once

#include <engine/ecs/systems/System.h>
#include <engine/ecs/components/TransformComponent.h>
#include <engine/ecs/components/SpriteComponent.h>

namespace Carrot::ECS {
    class SpriteRenderSystem: public RenderSystem<TransformComponent, Carrot::ECS::SpriteComponent>, public Identifiable<SpriteRenderSystem> {
    public:
        explicit SpriteRenderSystem(World& world): RenderSystem<TransformComponent, SpriteComponent>(world) {}
        explicit SpriteRenderSystem(const Carrot::DocumentElement& doc, World& world): SpriteRenderSystem(world) {}

        void onFrame(const Carrot::Render::Context& renderContext) override;

        void tick(double dt) override;

        std::unique_ptr<System> duplicate(World& newOwner) const override;

    public:
        inline static const char* getStringRepresentation() {
            return "SpriteRender";
        }

        virtual const char* getName() const override {
            return getStringRepresentation();
        }

    private:
        void updateSprite(Carrot::Render::Context renderContext, const TransformComponent& transform, Carrot::Render::Sprite& sprite);
    };
}

template<>
inline const char* Carrot::Identifiable<Carrot::ECS::SpriteRenderSystem>::getStringRepresentation() {
    return Carrot::ECS::SpriteRenderSystem::getStringRepresentation();
}