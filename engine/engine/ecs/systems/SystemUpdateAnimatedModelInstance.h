//
// Created by jglrxavpok on 27/02/2021.
//

#pragma once
#include "engine/ecs/components/Component.h"
#include "engine/ecs/components/AnimatedModelInstance.h"
#include "engine/ecs/systems/System.h"

namespace Carrot::ECS {
    class SystemUpdateAnimatedModelInstance: public RenderSystem<TransformComponent, AnimatedModelInstance>, public Identifiable<SystemUpdateAnimatedModelInstance> {
    public:
        explicit SystemUpdateAnimatedModelInstance(World& world): RenderSystem<TransformComponent, AnimatedModelInstance>(world) {}
        explicit SystemUpdateAnimatedModelInstance(const rapidjson::Value& json, World& world): SystemUpdateAnimatedModelInstance(world) {}

        void onFrame(Carrot::Render::Context renderContext) override;

        void tick(double dt) override;

    public:
        std::unique_ptr<System> duplicate(World& newOwner) const override;

    public:
        inline static const char* getStringRepresentation() {
            return "UpdateAnimatedModelInstance";
        }

        virtual const char* getName() const override {
            return getStringRepresentation();
        }

    protected:
        void onEntityAdded(Entity& entity) override;

    };

}

template<>
inline const char* Carrot::Identifiable<Carrot::ECS::SystemUpdateAnimatedModelInstance>::getStringRepresentation() {
    return Carrot::ECS::SystemUpdateAnimatedModelInstance::getStringRepresentation();
}