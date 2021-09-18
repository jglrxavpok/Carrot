//
// Created by jglrxavpok on 26/02/2021.
//

#pragma once

#include "Component.h"
#include "engine/render/InstanceData.h"

namespace Carrot::ECS {
    struct AnimatedModelInstance: public IdentifiableComponent<AnimatedModelInstance> {
        AnimatedInstanceData& instanceData;

        explicit AnimatedModelInstance(Entity entity, AnimatedInstanceData& instanceData): IdentifiableComponent<AnimatedModelInstance>(std::move(entity)), instanceData(instanceData) {};
    };
}