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

        /* TODO: Not serialisable at the moment
         * */

        const char *const getName() const override {
            return "AnimatedModelInstance";
        }

        std::unique_ptr<Component> duplicate(const Entity& newOwner) const override {
            return std::make_unique<AnimatedModelInstance>(newOwner, instanceData);
        }
    };
}

template<>
inline const char* Carrot::Identifiable<Carrot::ECS::AnimatedModelInstance>::getStringRepresentation() {
    return "AnimatedModelInstance";
}