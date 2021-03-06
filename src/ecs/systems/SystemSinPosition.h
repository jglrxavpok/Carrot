//
// Created by jglrxavpok on 06/03/2021.
//

#pragma once
#include "ecs/components/Component.h"
#include "ecs/components/Transform.h"
#include "ecs/components/ForceSinPosition.h"
#include "System.h"

namespace Carrot {
    class SystemSinPosition: public LogicSystem<Transform, ForceSinPosition> {
    private:
        double time = 0.0;

    public:
        explicit SystemSinPosition(Carrot::World& world): LogicSystem<Transform, ForceSinPosition>(world) {}

        void tick(double dt) override;
    };
}


