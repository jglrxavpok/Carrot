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
                savedComponents[savedIndex] = entity.getComponent(componentIDs[i])->serialise();
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
} // Peeler