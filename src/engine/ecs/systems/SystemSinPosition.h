//
// Created by jglrxavpok on 06/03/2021.
//

#pragma once
#include "engine/ecs/components/Component.h"
#include "engine/ecs/components/Transform.h"
#include "engine/ecs/components/ForceSinPosition.h"
#include "System.h"

namespace Carrot::ECS {
    class SystemSinPosition: public LogicSystem<Transform, ForceSinPosition> {
    private:
        double time = 0.0;

    public:
        explicit SystemSinPosition(World& world): LogicSystem<Transform, ForceSinPosition>(world) {}

        void tick(double dt) override;
    };
}


