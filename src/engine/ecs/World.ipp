#include "World.h"
#include <algorithm>

namespace Carrot::ECS {
    template<class Comp>
    Memory::OptionalRef<Comp> World::getComponent(const Entity& entity) const {
        auto componentMapLocation = this->entityComponents.find(entity.internalEntity);
        if(componentMapLocation == this->entityComponents.end()) {
            // no such entity
            return {};
        }

        auto& componentMap = componentMapLocation->second;
        auto componentLocation = componentMap.find(Comp::id);

        if(componentLocation == componentMap.end()) {
            // no such component
            return {};
        }
        return dynamic_cast<Comp*>(componentLocation->second.get());
    }

    template<typename Comp>
    Entity& Entity::addComponent(std::unique_ptr<Comp>&& component) {
        auto& componentMap = worldRef.entityComponents[internalEntity];
        componentMap[Comp::id] = std::move(component);
        return *this;
    }

    template<typename Comp, typename... Args>
    Entity& Entity::addComponent(Args&&... args) {
        auto& componentMap = worldRef.entityComponents[internalEntity];
        componentMap[Comp::id] = std::make_unique<Comp>(*this, args...);
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
        renderSystems.push_back(std::move(std::make_unique<RenderSystemType>(*this, args...)));
    }

    template<class LogicSystemType, typename... Args>
    void World::addLogicSystem(Args&&... args) {
        logicSystems.push_back(std::move(std::make_unique<LogicSystemType>(*this, args...)));
    }

    template<SystemType type, typename... RequiredComponents>
    void SignedSystem<type, RequiredComponents...>::forEachEntity(const std::function<void(Entity&, RequiredComponents&...)>& action) {
        for(auto& entity : entities) {
            if (entity) {
                action(entity, (world.getComponent<RequiredComponents>(entity).asRef())...);
            }
        }
    }

}