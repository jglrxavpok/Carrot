//
// Created by jglrxavpok on 19/03/2025.
//

#pragma once

#include <engine/ecs/components/Component.h>

namespace Carrot::ECS {
    struct MissingComponent: public IdentifiableComponent<MissingComponent> {
        Carrot::DocumentElement serializedVersion;
        std::string originalComponent;

        explicit MissingComponent(Entity entity, const std::string& originalComponent, const Carrot::DocumentElement& doc);

        Carrot::DocumentElement serialise() const override;

        const char *const getName() const override;

        std::unique_ptr<Component> duplicate(const Entity& newOwner) const override;

        virtual ComponentID getComponentTypeID() const override;
    };
}