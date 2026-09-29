//
// Created by jglrxavpok on 23/09/2026.
//

#pragma once
#include <engine/ecs/components/ComponentReflection.h>

namespace Carrot::ECS {
    struct PortalComponent: public ReflectionComponent<PortalComponent> {
        using ReflectionComponent::ReflectionComponent;

        FIELD(Carrot::ECS::Entity, otherPortal, "OtherPortal", {});
    };
}

ADD_COMPONENT_ID(Carrot::ECS, Portal);
