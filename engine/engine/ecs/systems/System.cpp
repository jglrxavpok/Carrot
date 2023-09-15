//
// Created by jglrxavpok on 27/02/2021.
//
#include "System.h"
#include <core/async/Counter.h>
#include <engine/Engine.h>
#include <engine/task/TaskScheduler.h>

namespace Carrot::ECS {
    System::System(World& world): world(world), signature() {}

    const Signature& System::getSignature() const {
        return signature;
    }

    std::span<const Entity> System::getEntities() const {
        return entities;
    }

    void System::onEntitiesAdded(const std::vector<EntityID>& added) {
        bool changed = false;
        for(const auto& e : added) {
            auto obj = Entity(e, world);
            auto it = std::find_if(entities.begin(), entities.end(), [&](const auto& entity) {
                return entity.getID() == e;
            });
            if(it != entities.end())
                continue;
            if((world.getSignature(obj) & getSignature()) == getSignature()) {
                changed = true;
                onEntityAdded(obj);
                entities.push_back(obj);
            }
        }

        if(changed) {
            recreateEntityWithComponentsList();
        }
    }

    void System::onEntitiesRemoved(const std::vector<EntityID>& removed) {
        bool changed = false;
        for(const auto& e : removed) {
            auto obj = Entity(e, world);
            std::size_t removedCount = std::erase_if(entities, [&](const Entity& entity) {
                return entity.operator EntityID() == e;
            });
            changed |= removedCount > 0;
        }

        if(changed) {
            recreateEntityWithComponentsList();
        }
    }

    void System::onEntitiesUpdated(const std::vector<EntityID>& updated) {
        bool changed = false;
        for(const auto& e : updated) {
            auto obj = Entity(e, world);

            if((world.getSignature(obj) & getSignature()) != getSignature()) {
                std::size_t removedCount = std::erase_if(entities, [&](const Entity& entity) {
                    return entity.operator EntityID() == e;
                });
                changed |= removedCount > 0;
            } else if((world.getSignature(obj) & getSignature()) == getSignature()) {
                auto it = std::find_if(entities.begin(), entities.end(), [&](const Entity& entity) {
                    return entity.operator EntityID() == e;
                });
                if(it == entities.end()) {
                    onEntityAdded(obj);
                    entities.push_back(obj);
                    changed = true;
                }
            }
        }

        if(changed || entities.size() != entitiesWithComponents.size()) {
            recreateEntityWithComponentsList();
        }
    }

    void System::parallelSubmit(const std::function<void()>& action, Async::Counter& counter) {
        TaskDescription description {
            .name = "parallelSubmit",
            .task = Async::AsTask(action),
            .joiner = &counter,
        };
        GetTaskScheduler().schedule(std::move(description), TaskScheduler::FrameParallelWork);
    }

    std::size_t System::concurrency() {
        return TaskScheduler::frameParallelWorkParallelismAmount();
    }

    void System::recreateEntityWithComponentsList() {
        entitiesWithComponents.resize(entities.size());
        std::size_t componentCount = signature.getComponentCount();

        for(std::size_t componentID = 0; componentID < MAX_COMPONENTS; componentID++) {
            if(signature.hasComponent(componentID)) {
                Signature::IndexType componentIndex = signature.getComponentIndex(componentID);

                for(std::size_t i = 0; i < entities.size(); i++) {
                    auto& withComponents = entitiesWithComponents[i];
                    const auto& entity = entities[i];
                    withComponents.entity = entity;
                    withComponents.components.resize(componentCount);

                    // if component hardcoded: create new object to reference it
                    // if not hardcoded: get from entity
                    Memory::OptionalRef<Component> component = entity.getComponent(componentID);
                    verify(component.hasValue(), "Component is not in entity??");
                    withComponents.components[componentIndex] = component.asPtr();
                }
            }
        }
    }

    SystemLibrary& getSystemLibrary() {
        static SystemLibrary lib;
        return lib;
    }
}