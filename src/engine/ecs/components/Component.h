//
// Created by jglrxavpok on 20/02/2021.
//

#pragma once
#include "engine/utils/Identifiable.h"
#include <engine/ecs/EntityTypes.h>

#include <utility>

namespace Carrot {

    class EasyEntity;

    struct Component {
    public:
        explicit Component(EasyEntity entity): entity(std::move(entity)) {}

        EasyEntity& getEntity() { return entity; }
        const EasyEntity& getEntity() const { return entity; }

        virtual ~Component() = default;

    private:
        EasyEntity entity;
    };

    template<class Self>
    struct IdentifiableComponent: public Component, Identifiable<Self> {
        explicit IdentifiableComponent(EasyEntity entity): Component(std::move(entity)) {}
    };

}
