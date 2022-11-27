//
// Created by jglrxavpok on 06/08/2021.
//

#pragma once

#include "System.h"
#include "engine/ecs/components/TransformComponent.h"

namespace Carrot::ECS {
    struct SystemTransformSwapBuffers: public LogicSystem<TransformComponent>, public Identifiable<SystemTransformSwapBuffers> {
    public:
        explicit SystemTransformSwapBuffers(World& world): LogicSystem<TransformComponent>(world) {}
        explicit SystemTransformSwapBuffers(const rapidjson::Value& json, World& world): SystemTransformSwapBuffers(world) {}

        virtual void swapBuffers() override;

        virtual void tick(double dt) override {}

        virtual std::unique_ptr<System> duplicate(World& newOwner) const override;

    public:
        inline static const char* getStringRepresentation() {
            return "TransformSwapBuffers";
        }

        virtual const char* getName() const override {
            return getStringRepresentation();
        }
    };
}

template<>
inline const char* Carrot::Identifiable<Carrot::ECS::SystemTransformSwapBuffers>::getStringRepresentation() {
    return Carrot::ECS::SystemTransformSwapBuffers::getStringRepresentation();
}