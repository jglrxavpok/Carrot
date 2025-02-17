//
// Created by jglrxavpok on 29/04/2024.
//

#include "Prefab.h"
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <engine/ecs/World.h>
#include <engine/Engine.h>

#include "components/PrefabInstanceComponent.h"

namespace Carrot::ECS {
    /*static*/ std::shared_ptr<Prefab> Prefab::makePrefab() {
        return std::shared_ptr<Prefab>(new Prefab);
    }

    void Prefab::load(TaskHandle& task, const Carrot::IO::VFS::Path& prefabAsset) {
        path = prefabAsset;
        internalScene.deserialise(prefabAsset);
    }

    /// Gets the given component inside this prefab, if it exists
    Memory::OptionalRef<Component> Prefab::getComponent(const Carrot::ECS::EntityID& childID, const ComponentID& componentID) const {
        return internalScene.world.getComponent(childID, componentID);
    }

    Memory::OptionalRef<Component> Prefab::getComponentByName(const Carrot::ECS::EntityID& childID, std::string_view componentName) const {
        TODO;
/*        for(const auto& [_, pComp] : components) {
            if(componentName == pComp->getName()) {
                return *pComp;
            }
        }*/
        return {};
    }

    Vector<Component*> Prefab::getAllComponents(const Carrot::ECS::EntityID& childID) const {
        return internalScene.world.getAllComponents(childID);
    }


    /// Save to disk. Can fail if the target path is not writtable.
    bool Prefab::save(const Carrot::IO::VFS::Path& prefabAsset) {
        this->path = prefabAsset;
        TODO; // save internal scene

        GetAssetServer().storePrefab(*this);
        return true;
    }

    /// Initializes this prefab with a copy of the contents of the given entity.
    void Prefab::createFromEntity(const Carrot::ECS::Entity& entity) {
        TODO;
    }

    /// Instantiate a copy of this prefab in the given world
    Entity Prefab::instantiate(World& world) const {
        verify(!path.isEmpty(), "Cannot instantiate a prefab which has no path (required because added to entities for edition & serialisation)");
        std::unordered_map<Carrot::ECS::EntityID, Carrot::ECS::EntityID> remap;

        TODO;
        /*
        std::function<Entity(const Prefab&, bool)> recurseInstantiate = [&](const Prefab& prefab, bool isRoot) -> Entity {
            Entity instantiated = prefab.instantiateOnlyThis(world);
            remap[prefab.getEntityID()] = instantiated.getID(); // clone will get a new id, so keep a mapping
            instantiated.addComponent<PrefabInstanceComponent>();
            PrefabInstanceComponent& component = instantiated.getComponent<PrefabInstanceComponent>();
            component.prefab = prefab.shared_from_this();
            component.isRoot = isRoot;

            for(const auto& [childID, pChild] : prefab.children) {
                verify(childID == pChild->getEntityID(), "Child ID and prefab entity ID do not match, this is a programming error!");
                Entity clonedChild = recurseInstantiate(*pChild, false);
                clonedChild.setParent(instantiated);
            }

            return instantiated;
        };

        Entity result = recurseInstantiate(*this, true);
        world.repairLinks(result, remap); // apply prefab->clone mapping to components referencing entities
        return result;*/
        return {};
    }

    const Carrot::IO::VFS::Path& Prefab::getVFSPath() const {
        return path;
    }

    void Prefab::applyChange(World& currentScene, Change& change) {
        forEachInstance(currentScene, [&](Carrot::ECS::Entity e) {
            change.apply(e);
        });
    }

    void Prefab::forEachInstance(World& currentScene, std::function<void(Carrot::ECS::Entity)> action) {
        for(auto& [entity, pComponents] : currentScene.queryEntities<PrefabInstanceComponent>()) {
            // first ensure we only modify instances of this prefab
            PrefabInstanceComponent& prefabInstanceComp = *reinterpret_cast<PrefabInstanceComponent*>(pComponents[0]);
            if(prefabInstanceComp.prefab.get() != this) {
                continue;
            }

            action(entity);
        }
    }

} // Carrot::ECS