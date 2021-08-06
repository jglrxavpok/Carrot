//
// Created by jglrxavpok on 26/02/2021.
//

#pragma once

#include "Component.h"
#include "engine/render/InstanceData.h"

namespace Carrot {
    struct AnimatedModelInstance: public IdentifiableComponent<AnimatedModelInstance> {
        AnimatedInstanceData& instanceData;

        explicit AnimatedModelInstance(EasyEntity entity, AnimatedInstanceData& instanceData): IdentifiableComponent<AnimatedModelInstance>(std::move(entity)), instanceData(instanceData) {};
    };
}