#pragma once

#include <engine/ecs/components/Component.h>

#include "ComponentReflection.h"

namespace Carrot::ECS {

    struct SoundListenerComponent : public Carrot::ECS::ReflectionComponent<SoundListenerComponent> {
        using ReflectionComponent::ReflectionComponent;
        FIELD(bool, active, "Active", true);
    };
}

ADD_COMPONENT_ID(Carrot::ECS, SoundListener);