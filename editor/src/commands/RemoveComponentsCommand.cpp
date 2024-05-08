//
// Created by jglrxavpok on 28/03/2024.
//

#include "RemoveComponentsCommand.h"

#include <Peeler.h>
#include <engine/ecs/Prefab.h>

namespace Peeler {
    RemoveComponentsCommand::RemoveComponentsCommand(Application& app, std::span<Carrot::ECS::EntityID> entityList, std::span<std::string> componentNames, std::span<Carrot::ComponentID> componentIDs)
    : ICommand(app, "Remove entity components")
    , componentNames(componentNames)
    , componentIDs(componentIDs)
    , entityList(entityList) {
        verify(componentIDs.size() == componentNames.size(), "Must be as many names as ids");
        savedComponents.resize(entityList.size() * componentNames.size());
        for (std::int64_t entityIndex = 0; entityIndex < entityList.size(); ++entityIndex) {
            Carrot::ECS::Entity entity = editor.currentScene.world.wrap(entityList[entityIndex]);
            for(std::int64_t i = 0; i < componentNames.size(); i++) {
                std::int64_t savedIndex = entityIndex * componentNames.size() + i;
                savedComponents[savedIndex] = entity.getComponent(componentIDs[i])->toJSON(globalDoc);
            }
        }
    }

    void RemoveComponentsCommand::undo() {
        for (std::int64_t entityIndex = 0; entityIndex < entityList.size(); ++entityIndex) {
            Carrot::ECS::Entity entity = editor.currentScene.world.wrap(entityList[entityIndex]);
            for(std::int64_t i = 0; i < componentNames.size(); i++) {
                std::int64_t savedIndex = entityIndex * componentNames.size() + i;
                std::unique_ptr<Carrot::ECS::Component> newComp = Carrot::ECS::getComponentLibrary().deserialise(
                    componentNames[i], savedComponents[savedIndex], entity);
                entity.addComponent(std::move(newComp));
            }
        }
    }

    void RemoveComponentsCommand::redo() {
        for (std::int64_t entityIndex = 0; entityIndex < entityList.size(); ++entityIndex) {
            Carrot::ECS::Entity e = editor.currentScene.world.wrap(entityList[entityIndex]);
            for(const Carrot::ComponentID& compID : componentIDs) {
                e.removeComponent(compID);
            }
        }
    }

    RemovePrefabComponentsCommand::RemovePrefabComponentsCommand(Application& app, std::shared_ptr<Carrot::ECS::Prefab> prefab, std::span<std::string> componentNames, std::span<Carrot::ComponentID> componentIDs)
    : ICommand(app, "Remove entity components")
    , prefab(prefab)
    , componentNames(componentNames)
    , componentIDs(componentIDs)
    {
        verify(componentIDs.size() == componentNames.size(), "Must be as many names as ids");
        savedComponents.resize(componentNames.size());
        for(std::int64_t i = 0; i < componentNames.size(); i++) {
            savedComponents[i] = prefab->getComponent(componentIDs[i])->toJSON(globalDoc);
        }

        Carrot::Vector<Carrot::ECS::EntityID> affectedEntities;
        affectedEntities.setGrowthFactor(2);
        prefab->forEachInstance(app.currentScene.world, [&](Carrot::ECS::Entity e) {
            affectedEntities.pushBack(e.getID());
        });
        pEntitiesCommand = Carrot::makeUnique<RemoveComponentsCommand>(Carrot::Allocator::getDefault(), app, affectedEntities, componentNames, componentIDs);
    }

    void RemovePrefabComponentsCommand::undo() {
        pEntitiesCommand->undo();
        for(std::int64_t i = 0; i < componentNames.size(); i++) {
            prefab->deserialiseAddComponent(componentNames[i], savedComponents[i]);
        }
    }

    void RemovePrefabComponentsCommand::redo() {
        for(const Carrot::ComponentID& compID : componentIDs) {
            prefab->removeComponent(compID);
        }
        pEntitiesCommand->redo();
    }
} // Peeler