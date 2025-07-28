//
// Created by jglrxavpok on 27/07/25.
//

#include "DeleteEntitiesCommand.h"

#include <Peeler.h>

namespace Peeler {

    DeleteEntitiesCommand::DeleteEntitiesCommand(Application& app, const std::unordered_set<Carrot::ECS::EntityID>& _entityList):
    ICommand(app, Carrot::sprintf("Deleted %d entities", _entityList.size())) {
        entityList.ensureReserve(_entityList.size());
        for (const auto& ent : _entityList) {
            entityList.emplaceBack() = ent;
        }
    }

    void DeleteEntitiesCommand::redo() {
        serializedDeleted.ensureReserve(entityList.size());
        serializedDeleted.clear();

        Carrot::ECS::World& world = editor.currentScene.world;
        // TODO: topological sort?
        for (const auto& e : entityList) {
            SerializedEntity& serializedEntity = serializedDeleted.emplaceBack();
            Carrot::ECS::Entity entity = world.wrap(e);
            serializedEntity.id = entity.getID();
            serializedEntity.flags = entity.getFlags();
            serializedEntity.name = entity.getName();
            std::optional<Carrot::ECS::EntityID> parent = entity.getParent();
            if (parent.has_value()) {
                serializedEntity.parent = parent.value();
            }

            for (const auto* pComponent : entity.getAllComponents()) {
                serializedEntity.componentNames.emplaceBack() = pComponent->getName();
                serializedEntity.components.emplaceBack() = pComponent->serialise();
            }
        }

        for (const auto& e : entityList) {
            world.removeEntity(e);
        }

        editor.selectedEntityIDs.clear();
        editor.markDirty();
    }

    void DeleteEntitiesCommand::undo() {
        Carrot::ECS::World& world = editor.currentScene.world;
        for (const auto& serialized : serializedDeleted) {
            Carrot::ECS::Entity entity = world.newEntityWithID(serialized.id, serialized.name);
            entity.setFlags(serialized.flags);
            if (serialized.parent != Carrot::ECS::EntityID::null()) {
                entity.setParent(world.wrap(serialized.parent));
            }

            for(std::int64_t i = 0; i < serialized.components.size(); i++) {
                std::unique_ptr<Carrot::ECS::Component> newComp = Carrot::ECS::getComponentLibrary().deserialise(
                    serialized.componentNames[i], serialized.components[i], entity);
                entity.addComponent(std::move(newComp));
            }
        }

        editor.selectedEntityIDs.clear();
        for (const auto& e : entityList) {
            editor.selectedEntityIDs.insert(e);
        }
        editor.markDirty();
    }

} // Peeler