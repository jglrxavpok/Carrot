#include "World.h"
#include <algorithm>

template<class Comp>
Comp* Carrot::World::getComponent(Entity_Ptr& entity) const {
    auto componentMapLocation = this->entityComponents.find(entity);
    if(componentMapLocation == this->entityComponents.end()) {
        // no such entity
        return nullptr;
    }

    auto& componentMap = componentMapLocation->second;
    auto componentLocation = componentMap.find(Comp::id);

    if(componentLocation == componentMap.end()) {
        // no such component
        return nullptr;
    }
    return dynamic_cast<Comp*>(componentLocation->second.get());
}

template<typename Comp>
Carrot::World::EasyEntity& Carrot::World::EasyEntity::addComponent(unique_ptr<Comp>&& component) {
    auto& componentMap = worldRef.entityComponents[internalEntity];
    componentMap[Comp::id] = move(component);
}

template<typename Comp, typename... Args>
Carrot::World::EasyEntity& Carrot::World::EasyEntity::addComponent(Args... args) {
    auto& componentMap = worldRef.entityComponents[internalEntity];
    componentMap[Comp::id] = move(make_unique<Comp>(args...));
    return *this;
}

template<typename Comp>
Comp* Carrot::World::EasyEntity::getComponent() {
    return worldRef.getComponent<Comp>(internalEntity);
}