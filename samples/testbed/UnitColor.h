//
// Created by jglrxavpok on 20/02/2021.
//

#pragma once

#include "engine/ecs/components/Component.h"

namespace Game {
    namespace Unit {
        enum class Type {
            Red,
            Green,
            Blue,
        };
    }

    struct UnitColor: public Carrot::ECS::IdentifiableComponent<UnitColor> {
        Unit::Type color;

        explicit UnitColor(Carrot::ECS::Entity entity, Unit::Type color): Carrot::ECS::IdentifiableComponent<UnitColor>(std::move(entity)), color(color) {}
    };
}
