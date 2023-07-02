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
        auto& componentMap = getWorld().entityComponents[internalEntity];
        componentMap[component->getComponentTypeID()] = std::move(component);
        getWorld().entitiesUpdated.push_back(internalEntity);
        return *this;
    }

    template<typename Comp, typename... Args>
    Entity& Entity::addComponent(Args&&... args) {
        auto& componentMap = getWorld().entityComponents[internalEntity];
        componentMap[Comp::getID()] = std::make_unique<Comp>(*this, args...);
        getWorld().entitiesUpdated.push_back(internalEntity);
        return *this;
    }

    template<typename Comp>
    Entity& Entity::removeComponent() {
        auto& componentMap = getWorld().entityComponents[internalEntity];
        componentMap.erase(Comp::getID());
        getWorld().entitiesUpdated.push_back(internalEntity);
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
        return getWorld().getComponent<Comp>(*this);
    }

    template<typename Component>
    std::vector<Entity> World::getEntitiesWith() const {
        std::vector<Entity> result;
        for(auto& [entityID, components] : entityComponents) {
            auto componentLocation = components.find(Component::getID());

            if(componentLocation == components.end()) {
                continue;
            }

            result.push_back(wrap(entityID));
        }
        return result;
    }

    template<typename... Component>
    std::vector<Entity> World::queryEntities() const {
        std::unordered_set<Carrot::ComponentID> ids;
        (ids.insert(Component::getID()), ...);
        return queryEntities(ids);
    }

    template<class RenderSystemType, typename... Args>
    RenderSystemType& World::addRenderSystem(Args&&... args) {
        auto system = std::make_unique<RenderSystemType>(*this, args...);
        auto& result = *system;
        addRenderSystem(std::move(system));
        return result;
    }

    template<class LogicSystemType, typename... Args>
    LogicSystemType& World::addLogicSystem(Args&&... args) {
        auto system = std::make_unique<LogicSystemType>(*this, args...);
        auto& result = *system;
        addLogicSystem(std::move(system));
        return result;
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
        if(entities.empty())
            return;
        Async::Counter counter;
        const std::size_t entityCount = entities.size();
        const std::size_t stepSize = static_cast<std::size_t>(ceil((double)entityCount / concurrency()));
        for(std::size_t index = 0; index < entityCount; index += stepSize) {
            parallelSubmit([&, startIndex = index, endIndex = index + stepSize -1]() {
                for(std::size_t localIndex = startIndex; localIndex <= endIndex && localIndex < entityCount; localIndex++) {
                    auto& entity = entities[localIndex];
                    if (entity) {
                        action(entity, (world.getComponent<RequiredComponents>(entity).asRef())...);
                    }
                }
            }, counter);
        }

        counter.busyWait();
    }
}