//
// Created by jglrxavpok on 18/03/2024.
//

#pragma once

#include <commands/UndoStack.h>
#include <engine/ecs/EntityTypes.h>

namespace Peeler {

    class RenameEntitiesCommand: public ICommand {
    public:
        RenameEntitiesCommand(Application& app, std::span<Carrot::ECS::EntityID> entityList, const std::string& newName);

        virtual void undo() override;
        virtual void redo() override;

    private:
        Carrot::Vector<Carrot::ECS::EntityID> entityList;
        Carrot::Vector<std::string> oldNames;
        std::string newName;
    };

} // Peeler
