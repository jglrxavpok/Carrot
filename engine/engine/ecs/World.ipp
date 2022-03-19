#include "World.h"
#include <algorithm>
#include <core/async/Counter.h>

namespace Carrot::ECS {
    template<class Comp>
    Memory::OptionalRef<Comp> World::getComponent(const Entity& entity) const {
        return getComponent<Comp>(entity.getID());
    }

    template<class Comp>
    Memory::OptionalRef<Comp> World::getComponent(const EntityID& entityID) const {
        auto componentMapLocation = this->entityComponents.find(entityID);
        if(componentMapLocation == this->entityComponents.end()) {
            // no such entity
            return {};
        }

        auto& componentMap = componentMapLocation->second;
        auto componentLocation = componentMap.find(Comp::getID());

        if(componentLocation == componentMap.end()) {
            // no such component
            return {};
        }
        return dynamic_cast<Comp*>(componentLocation->second.get());
    }

    template<typename Comp>
    Entity& Entity::addComponent(std::unique_ptr<Comp>&& component) {
        auto& componentMap = worldRef.entityComponents[internalEntity];
        componentMap[component->getComponentTypeID()] = std::move(component);
        worldRef.entitiesUpdated.push_back(internalEntity);
        return *this;
    }

    template<typename Comp, typename... Args>
    Entity& Entity::addComponent(Args&&... args) {
        auto& componentMap = worldRef.entityComponents[internalEntity];
        componentMap[Comp::getID()] = std::make_unique<Comp>(*this, args...);
        worldRef.entitiesUpdated.push_back(internalEntity);
        return *this;
    }

    template<typename Comp>
    Entity& Entity::removeComponent() {
        auto& componentMap = worldRef.entityComponents[internalEntity];
        componentMap.erase(Comp::getID());
        worldRef.entitiesUpdated.push_back(internalEntity);
        return *this;
    }

    template<typename Comp, typename... Args>
    Entity& Entity::addComponentIf(bool condition, Args&&... args) {
        if(condition) {
            addComponent<Comp>(std::forward<Args>(args)...);
        }
        return *this;
    }

    template<typename Comp>
    Memory::OptionalRef<Comp> Entity::getComponent() {
        return worldRef.getComponent<Comp>(*this);
    }

    template<class RenderSystemType, typename... Args>
    void World::addRenderSystem(Args&&... args) {
        addRenderSystem(std::move(std::make_unique<RenderSystemType>(*this, args...)));
    }

    template<class LogicSystemType, typename... Args>
    void World::addLogicSystem(Args&&... args) {
        addLogicSystem(std::move(std::make_unique<LogicSystemType>(*this, args...)));
    }

    template<class LogicSystemType>
    void World::removeLogicSystem() {
        Carrot::removeIf(logicSystems, [&](auto& ptr) {
            return typeid(*ptr) == typeid(LogicSystemType);
        });
    }

    template<class RenderSystemType>
    void World::removeRenderSystem() {
        Carrot::removeIf(renderSystems, [&](auto& ptr) {
            return typeid(*ptr) == typeid(RenderSystemType);
        });
    }

    template<class LogicSystemType>
    LogicSystemType* World::getLogicSystem() {
        for(auto& system : logicSystems) {
            if(typeid(*system) == typeid(LogicSystemType)) {
                return static_cast<LogicSystemType*>(system.get());
            }
        }
        return nullptr;
    }

    template<class RenderSystemType>
    RenderSystemType* World::getRenderSystem() {
        for(auto& system : renderSystems) {
            if(typeid(*system) == typeid(RenderSystemType)) {
                return static_cast<RenderSystemType*>(system.get());
            }
        }
        return nullptr;
    }

    template<SystemType type, typename... RequiredComponents>
    void SignedSystem<type, RequiredComponents...>::forEachEntity(const std::function<void(Entity&, RequiredComponents&...)>& action) {
        for(auto& entity : entities) {
            if (entity) {
                action(entity, (world.getComponent<RequiredComponents>(entity).asRef())...);
            }
        }
    }

    template<SystemType type, typename... RequiredComponents>
    void SignedSystem<type, RequiredComponents...>::parallelForEachEntity(const std::function<void(Entity&, RequiredComponents&...)>& action) {
        Async::Counter counter;
        for(auto& entity : entities) {
            parallelSubmit([&]() {
                if (entity) {
                    action(entity, (world.getComponent<RequiredComponents>(entity).asRef())...);
                }
            }, counter);
        }

        counter.busyWait();
    }
}