//
// Created by jglrxavpok on 27/02/2021.
//
#include "System.h"

Carrot::System::System(Carrot::World& world): world(world), signature() {}

const Carrot::Signature& Carrot::System::getSignature() const {
    return signature;
}

void Carrot::System::onEntitiesAdded(const vector<Entity_Ptr>& added) {
    for(const auto& e : added) {
        if((world.getSignature(e) & getSignature()) == getSignature()) {
            auto ptr = weak_ptr<Entity>(e);
            onEntityAdded(ptr);
            entities.emplace_back(ptr);
        }
    }
}

void Carrot::System::onEntitiesRemoved(const vector<Entity_Ptr>& removed) {
    for(const auto& e : removed) {
        entities.erase(find_if(entities.begin(), entities.end(), [&](const auto& entity) {
            if(auto ent = entity.lock()) {
                return *ent == *e;
            }
            return false;
        }));
    }
}