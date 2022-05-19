//
// Created by jglrxavpok on 06/03/2021.
//

#pragma once
#include "engine/ecs/components/Component.h"
#include "engine/ecs/components/TransformComponent.h"
#include "engine/ecs/components/LightComponent.h"
#include "System.h"

namespace Carrot::ECS {
    class SystemHandleLights: public RenderSystem<TransformComponent, LightComponent>, public Identifiable<SystemHandleLights> {
    public:
        explicit SystemHandleLights(World& world): RenderSystem<TransformComponent, LightComponent>(world) {}
        explicit SystemHandleLights(const rapidjson::Value& json, World& world): SystemHandleLights(world) {}

        void onFrame(Carrot::Render::Context) override;

        std::unique_ptr<System> duplicate(World& newOwner) const override;

        void reload() override;

        void unload() override;

    public:
        inline static const char* getStringRepresentation() {
            return "HandleLights";
        }

        virtual const char* getName() const override {
            return getStringRepresentation();
        }
    };
}

template<>
inline const char* Carrot::Identifiable<Carrot::ECS::SystemHandleLights>::getStringRepresentation() {
    return Carrot::ECS::SystemHandleLights::getStringRepresentation();
}