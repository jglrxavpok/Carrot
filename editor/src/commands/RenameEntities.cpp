//
// Created by jglrxavpok on 18/03/2024.
//

#include "RenameEntities.h"

#include <Peeler.h>

namespace Peeler {

    RenameEntitiesCommand::RenameEntitiesCommand(Application& app, std::span<Carrot::ECS::EntityID> _entityList, const std::string& _newName)
    : ICommand(app, Carrot::sprintf("Rename entities to '%s'", _newName.c_str())) {
        entityList = _entityList;
        newName = _newName;
        oldNames.ensureReserve(_entityList.size());
        for(const auto& entityID : _entityList) {
            oldNames.pushBack(editor.currentScene.world.getName(entityID));
        }
    }


    void RenameEntitiesCommand::undo() {
        for(std::int64_t i = 0; i < entityList.size(); i++) {
            editor.currentScene.world.getName(entityList[i]) = oldNames[i];
        }
    }

    void RenameEntitiesCommand::redo() {
        for(const auto& e : entityList) {
            editor.currentScene.world.getName(e) = newName;
        }
    }
} // Peeler