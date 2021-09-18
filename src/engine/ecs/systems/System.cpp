//
// Created by jglrxavpok on 27/02/2021.
//
#include "System.h"

namespace Carrot::ECS {
    System::System(World& world): world(world), signature() {}

    const Signature& System::getSignature() const {
        return signature;
    }

    void System::onEntitiesAdded(const std::vector<EntityID>& added) {
        for(const auto& e : added) {
            auto obj = Entity(e, world);
            if((world.getSignature(obj) & getSignature()) == getSignature()) {
                onEntityAdded(obj);
                entities.push_back(obj);
            }
        }
    }

    void System::onEntitiesRemoved(const std::vector<EntityID>& removed) {
        for(const auto& e : removed) {
            auto obj = Entity(e, world);
            entities.erase(std::remove_if(entities.begin(), entities.end(), [&](const Entity& entity) {
                return entity.operator EntityID() == e;
            }), entities.end());
        }
    }
}