//
// Created by jglrxavpok on 27/07/25.
//

#pragma once
#include <unordered_set>
#include <engine/ecs/EntityTypes.h>

#include "UndoStack.h"

namespace Peeler {

    class DuplicateEntitiesCommand: public ICommand {
    public:
        DuplicateEntitiesCommand(Application& app, const std::unordered_set<Carrot::ECS::EntityID>& _entityList);

        void redo() override;
        void undo() override;

    private:
        Carrot::Vector<Carrot::ECS::EntityID> entityList;
        Carrot::Vector<Carrot::ECS::EntityID> duplicatedEntities;

        // IDs need to be stable when undoing then redoing this command, to ensure commands following this one use the correct IDs
        std::unordered_map<Carrot::ECS::EntityID, Carrot::ECS::EntityID> remap;
    };

} // Peeler