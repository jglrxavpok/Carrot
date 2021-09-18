//
// Created by jglrxavpok on 06/03/2021.
//

#pragma once
#include "engine/ecs/components/Component.h"
#include "engine/ecs/components/Transform.h"
#include "engine/ecs/components/Kinematics.h"
#include "System.h"

namespace Carrot::ECS {
    class SystemKinematics: public LogicSystem<Transform, Kinematics> {
    public:
        explicit SystemKinematics(World& world): LogicSystem<Transform, Kinematics>(world) {}

        void tick(double dt) override;
    };
}