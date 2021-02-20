//
// Created by jglrxavpok on 20/02/2021.
//

#include "World.h"

Carrot::World::EasyEntity Carrot::World::newEntity() {
    auto newID = freeEntityID++;
    auto entity = make_shared<Entity>(newID);
    auto toReturn = EasyEntity(entity, *this);
    entitiesToAdd.emplace_back(entity);
    return toReturn;
}

void Carrot::World::tick(double dt) {
    for(const auto& toAdd : entitiesToAdd) {
        entities.push_back(toAdd);
    }
    for(const auto& toRemove : entitiesToRemove) {
        auto position = find(entities.begin(), entities.end(), toRemove);
        if(position != entities.end()) { // clear components
            entityComponents.erase(entityComponents.find(toRemove));
        }
    }
    entitiesToAdd.clear();
    entitiesToRemove.clear();

    // TODO: tick systems
}