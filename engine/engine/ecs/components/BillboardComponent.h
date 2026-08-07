#pragma once

#include <engine/ecs/components/ComponentReflection.h>

namespace Carrot::ECS {
    /**
     * Used to tag entities that should always face the camera
     */
    struct BillboardComponent : public Carrot::ECS::ReflectionComponent<BillboardComponent> {
        using ReflectionComponent::ReflectionComponent;
    };
}

ADD_COMPONENT_ID(Carrot::ECS, Billboard);