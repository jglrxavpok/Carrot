//
// Created by jglrxavpok on 28/03/2024.
//

#pragma once

#include <commands/UndoStack.h>
#include <engine/ecs/EntityTypes.h>
#include <rapidjson/document.h>

namespace Peeler {
    /**
     * Removes components from a list of entities.
     * These components are referenced by both their ComponentID (for easy removal) and their Name
     * (which is used by ComponentLibrary to recreate the correct component)
     */
    class RemoveComponentsCommand: public ICommand {
    public:
        RemoveComponentsCommand(Application& app, std::span<Carrot::ECS::EntityID> entityList, std::span<std::string> componentNames, std::span<Carrot::ComponentID> componentIDs);

        virtual void undo() override;
        virtual void redo() override;

    private:
        rapidjson::Document globalDoc; // TODO: replace with allocator
        Carrot::Vector<Carrot::ECS::EntityID> entityList;
        Carrot::Vector<Carrot::ComponentID> componentIDs;
        Carrot::Vector<std::string> componentNames;
        Carrot::Vector<rapidjson::Value> savedComponents;
    };
    /**
     * Removes components from a prefab, this affects instance of the prefab.
     * See RemoveComponentsCommand
     */
    class RemovePrefabComponentsCommand: public ICommand {
    public:
        RemovePrefabComponentsCommand(Application& app, std::shared_ptr<Carrot::ECS::Prefab> prefab, std::span<std::string> componentNames, std::span<Carrot::ComponentID> componentIDs);

        virtual void undo() override;
        virtual void redo() override;

    private:
        rapidjson::Document globalDoc; // TODO: replace with allocator
        Carrot::UniquePtr<RemoveComponentsCommand> pEntitiesCommand;
        std::shared_ptr<Carrot::ECS::Prefab> prefab;
        Carrot::Vector<Carrot::ComponentID> componentIDs;
        Carrot::Vector<std::string> componentNames;
        Carrot::Vector<rapidjson::Value> savedComponents;
    };

} // Peeler
