//
// Created by jglrxavpok on 27/07/25.
//

#pragma once
#include <unordered_set>
#include <core/io/Document.h>
#include <engine/ecs/EntityTypes.h>

#include "UndoStack.h"

namespace Peeler {

    class DeleteEntitiesCommand: public ICommand {
    public:
        DeleteEntitiesCommand(Application& app, const std::unordered_set<Carrot::ECS::EntityID>& entityList);

        void redo() override;
        void undo() override;

    private:
        struct SerializedEntity {
            std::string name;
            Carrot::ECS::EntityID id = Carrot::ECS::EntityID::null();
            Carrot::ECS::EntityID parent = Carrot::ECS::EntityID::null();
            Carrot::ECS::EntityFlags flags;

            Carrot::Vector<std::string> componentNames;
            Carrot::Vector<Carrot::DocumentElement> components;
        };

        Carrot::Vector<Carrot::ECS::EntityID> entityList;
        Carrot::Vector<SerializedEntity> serializedDeleted; //< to allow redo
    };

} // Peeler