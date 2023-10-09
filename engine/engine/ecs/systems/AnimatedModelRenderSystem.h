//
// Created by jglrxavpok on 8/10/2023.
//

#pragma once

#include <engine/ecs/systems/System.h>
#include <engine/ecs/components/TransformComponent.h>
#include <engine/ecs/components/AnimatedModelComponent.h>

namespace Carrot::ECS {
    class AnimatedModelRenderSystem: public RenderSystem<TransformComponent, Carrot::ECS::AnimatedModelComponent>, public Identifiable<AnimatedModelRenderSystem> {
    public:
        explicit AnimatedModelRenderSystem(World& world): RenderSystem<TransformComponent, AnimatedModelComponent>(world) {}
        explicit AnimatedModelRenderSystem(const rapidjson::Value& json, World& world);

        void onFrame(Carrot::Render::Context renderContext) override;
        void tick(double deltaTime) override;

        std::unique_ptr<System> duplicate(World& newOwner) const override;

        void reload() override;

        void unload() override;

    public:
        inline static const char* getStringRepresentation() {
            return "AnimatedModelRender";
        }

        virtual const char* getName() const override {
            return getStringRepresentation();
        }

    private:
        //< list of AnimatedModel rendered during a call to onFrame. Declared inside class to keep memory from a frame to another, and avoid allocations
        std::unordered_set<Render::AnimatedModel*> renderedModels;
        double time = 0.0;
    };
}

template<>
inline const char* Carrot::Identifiable<Carrot::ECS::AnimatedModelRenderSystem>::getStringRepresentation() {
    return Carrot::ECS::AnimatedModelRenderSystem::getStringRepresentation();
}