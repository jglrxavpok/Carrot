//
// Created by jglrxavpok on 28/03/2024.
//

#include "AddComponentsCommand.h"

#include <Peeler.h>

namespace Peeler {
    AddComponentsCommand::AddComponentsCommand(Application& app, std::span<Carrot::ECS::EntityID> _entityList, std::span<std::string> componentNames, std::span<Carrot::ComponentID> componentIDs)
    : ICommand(app, "Add entity components")
    , componentNames(componentNames)
    , componentIDs(componentIDs) {
        verify(componentIDs.size() == componentNames.size(), "Must be as many names as ids");
        entityList.ensureReserve(_entityList.size());
        for (std::int64_t entityIndex = 0; entityIndex < _entityList.size(); ++entityIndex) {
            Carrot::ECS::Entity e = editor.currentScene.world.wrap(_entityList[entityIndex]);
            for(std::int64_t i = 0; i < componentNames.size(); i++) {
                // don't override if there is already a component with this ID
                // (case where we try to add to multiple entities at once, and one of them already has the component)
                if(!e.getComponent(componentIDs[i]).hasValue()) {
                    entityList.pushBack(e);
                }
            }
        }
    }

    void AddComponentsCommand::undo() {
        for (std::int64_t entityIndex = 0; entityIndex < entityList.size(); ++entityIndex) {
            Carrot::ECS::Entity entity = editor.currentScene.world.wrap(entityList[entityIndex]);
            for(std::int64_t i = 0; i < componentNames.size(); i++) {
                entity.removeComponent(componentIDs[i]);
            }
        }
    }

    void AddComponentsCommand::redo() {
        for (std::int64_t entityIndex = 0; entityIndex < entityList.size(); ++entityIndex) {
            Carrot::ECS::Entity e = editor.currentScene.world.wrap(entityList[entityIndex]);
            for(std::int64_t i = 0; i < componentNames.size(); i++) {
                auto comp = Carrot::ECS::getComponentLibrary().create(componentNames[i], e);
                e.addComponent(std::move(comp));
            }
        }
    }
} // Peeler