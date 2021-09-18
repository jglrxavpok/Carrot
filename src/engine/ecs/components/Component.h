//
// Created by jglrxavpok on 20/02/2021.
//

#pragma once
#include "engine/utils/Identifiable.h"
#include <engine/ecs/EntityTypes.h>

#include <utility>

namespace Carrot::ECS {

    class Entity;

    struct Component {
    public:
        explicit Component(Entity entity): entity(std::move(entity)) {}

        Entity& getEntity() { return entity; }
        const Entity& getEntity() const { return entity; }

        virtual ~Component() = default;

    private:
        Entity entity;
    };

    template<class Self>
    struct IdentifiableComponent: public Component, Identifiable<Self> {
        explicit IdentifiableComponent(Entity entity): Component(std::move(entity)) {}
    };

}
