//
// Created by jglrxavpok on 28/03/2024.
//

#pragma once

#include <commands/UndoStack.h>
#include <engine/ecs/EntityTypes.h>
#include <rapidjson/document.h>
#include <unordered_set>

namespace Peeler {
    /**
     * Adds components to a list of entities.
     * These components are referenced by both their ComponentID (for easy removal) and their Name
     * (which is used by ComponentLibrary to recreate the correct component)
     */
    class AddComponentsCommand: public ICommand {
    public:
        AddComponentsCommand(Application& app, std::unordered_set<Carrot::ECS::EntityID> entityList, std::span<std::string> componentNames, std::span<Carrot::ComponentID> componentIDs);

        virtual void undo() override;
        virtual void redo() override;

    private:
        Carrot::Vector<Carrot::ECS::EntityID> entityList;
        Carrot::Vector<Carrot::ComponentID> componentIDs;
        Carrot::Vector<std::string> componentNames;
    };

} // Peeler
