//
// Created by jglrxavpok on 27/07/25.
//

#include "DuplicateEntitiesCommand.h"

#include <Peeler.h>

namespace Peeler {

    DuplicateEntitiesCommand::DuplicateEntitiesCommand(Application& app, const std::unordered_set<Carrot::ECS::EntityID>& _entityList):
        ICommand(app, Carrot::sprintf("Duplicate %d entities", _entityList.size())) {
        entityList.ensureReserve(_entityList.size());
        duplicatedEntities.ensureReserve(_entityList.size());
        for (auto& entity : _entityList) {
            entityList.emplaceBack() = entity;
        }
    }

    void DuplicateEntitiesCommand::redo() {
        duplicatedEntities.clear();

        std::unordered_set<Carrot::ECS::EntityID> newSelectedEntities;
        for (const auto& selectedEntityID : entityList) {
            auto entity = editor.currentScene.world.wrap(selectedEntityID);
            auto clone = editor.currentScene.world.duplicateWithRemap(entity, entity.getParent(), remap);
            duplicatedEntities.emplaceBack() = clone.getID();
            newSelectedEntities.insert(clone.getID());
        }

        editor.selectedEntityIDs = newSelectedEntities;
        editor.markDirty();
    }

    void DuplicateEntitiesCommand::undo() {
        editor.selectedEntityIDs.clear();
        verify(entityList.size() == duplicatedEntities.size(), "Command was never executed?");
        Carrot::ECS::World& world = editor.currentScene.world;
        for (i32 index = 0; index < entityList.size(); index++) {
            world.removeEntity(duplicatedEntities[index]);
            editor.selectedEntityIDs.insert(entityList[index]);
        }

        editor.markDirty();
    }

} // Peeler