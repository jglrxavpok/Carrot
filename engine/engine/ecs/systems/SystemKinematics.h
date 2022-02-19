//
// Created by jglrxavpok on 06/03/2021.
//

#pragma once
#include "engine/ecs/components/Component.h"
#include "engine/ecs/components/TransformComponent.h"
#include "engine/ecs/components/Kinematics.h"
#include "System.h"

namespace Carrot::ECS {
    class SystemKinematics: public LogicSystem<TransformComponent, Kinematics>, public Identifiable<SystemKinematics> {
    public:
        explicit SystemKinematics(World& world): LogicSystem<TransformComponent, Kinematics>(world) {}
        explicit SystemKinematics(const rapidjson::Value& json, World& world): SystemKinematics(world) {}

        void tick(double dt) override;

        std::unique_ptr<System> duplicate(World& newOwner) const override;

    public:
        inline static const char* getStringRepresentation() {
            return "Kinematics";
        }

        virtual const char* getName() const override {
            return getStringRepresentation();
        }
    };
}