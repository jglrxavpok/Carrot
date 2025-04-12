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
#include "components/TransformComponent.h"

namespace Carrot::ECS {
    /*static*/ std::shared_ptr<Prefab> Prefab::makePrefab() {
        return std::shared_ptr<Prefab>(new Prefab);
    }

    std::unordered_set<Carrot::ECS::EntityID> Prefab::getChildrenIDs(const EntityID& prefabChildID) const {
        std::unordered_set<Carrot::ECS::EntityID> result;
        if (prefabChildID == PrefabRootUUID) { // scene root
            for (const auto& e : internalScene.world.getAllEntities()) {
                if (e.getParent().has_value()) {
                    continue;
                }

                result.insert(e.getID());
            }
        } else {
            Entity subtree = internalScene.world.wrap(prefabChildID);
            for (const auto& child : subtree.getChildren(ShouldRecurse::NoRecursion)) {
                result.insert(child.getID());
            }
        }

        return result;
    }

    void Prefab::load(const Carrot::IO::VFS::Path& prefabAsset) {
        path = prefabAsset;
        internalScene.deserialise(prefabAsset);
        internalScene.world.flushEntityCreationAndRemoval(); // ensure entities are properly added
        internalScene.unload(); // deactivate default-active elements (like animated model handles)
    }

    bool Prefab::hasChildWithID(const Carrot::ECS::EntityID& childID) const {
        if (childID == PrefabRootUUID) {
            return true;
        }
        return internalScene.world.exists(childID);
    }

    /// Gets the given component inside this prefab, if it exists
    Memory::OptionalRef<Component> Prefab::getComponent(const Carrot::ECS::EntityID& childID, const ComponentID& componentID) const {
        if (childID == PrefabRootUUID) { // scene root
            return {};
        }
        return internalScene.world.getComponent(childID, componentID);
    }

    Memory::OptionalRef<Component> Prefab::getComponentByName(const Carrot::ECS::EntityID& childID, std::string_view componentName) const {
        if (childID == PrefabRootUUID) { // scene root
            return {};
        }
        Entity e = internalScene.world.wrap(childID);
        if (!e.exists()) {
            return {};
        }
        for(const auto& pComp : e.getAllComponents()) {
            if(componentName == pComp->getName()) {
                return *pComp;
            }
        }
        return {};
    }

    Vector<Component*> Prefab::getAllComponents(const Carrot::ECS::EntityID& childID) const {
        return internalScene.world.getAllComponents(childID);
    }

    std::vector<const System*> Prefab::getBaseSceneLogicSystems() const {
        return internalScene.world.getLogicSystems();
    }
    std::vector<const System*> Prefab::getBaseSceneRenderSystems() const {
        return internalScene.world.getRenderSystems();
    }

    /// Save to disk. Can fail if the target path is not writtable.
    bool Prefab::save(const Carrot::IO::VFS::Path& prefabAsset) {
        this->path = prefabAsset;
        internalScene.serialise(GetVFS().resolve(prefabAsset));

        GetAssetServer().storePrefab(*this);
        return true;
    }

    /// Initializes this prefab with a copy of the contents of the given entity.
    void Prefab::createFromEntity(const Carrot::ECS::Entity& entity) {
        internalScene.clear();

        // systems are stored to warn user if prefab is used in a context where these systems are not present
        for (const System* pSystem : entity.getWorld().getRenderSystems()) {
            if (!pSystem->shouldBeSerialized()) {
                continue;
            }

            internalScene.world.addRenderSystem(pSystem->duplicate(internalScene.world));
        }
        for (const System* pSystem : entity.getWorld().getLogicSystems()) {
            if (!pSystem->shouldBeSerialized()) {
                continue;
            }

            internalScene.world.addLogicSystem(pSystem->duplicate(internalScene.world));
        }

        std::function<void(const Entity&, const EntityID&)> copyRecursively = [&](const Entity& toCopy, const EntityID& parentID) {
            Entity copy = internalScene.world.newEntity(toCopy.getName());
            copy.setFlags(toCopy.getFlags());
            for (const Component* pComp : toCopy.getAllComponents()) {
                if (!pComp->isSerializable()) {
                    continue;
                }

                copy.addComponent(pComp->duplicate(copy));
            }

            if (parentID != PrefabRootUUID) {
                copy.setParent(internalScene.world.wrap(parentID));
            }

            for (const auto& child : toCopy.getChildren(ShouldRecurse::NoRecursion)) {
                copyRecursively(child, copy.getID());
            }
        };
        copyRecursively(entity, PrefabRootUUID);
        internalScene.world.tick(0.0f); // ensure entities are properly added
    }

    Entity Prefab::instantiateSubTree(World& world, const Carrot::ECS::EntityID& startingPoint, std::unordered_map<Carrot::ECS::EntityID, Carrot::ECS::EntityID>& remap) const {
        Entity startEntity = internalScene.world.wrap(startingPoint);
        if (!startEntity.exists()) {
            return world.wrap(Carrot::UUID::null());
        }

        Entity cloned = world.newEntity(startEntity.getName());
        cloned.setFlags(startEntity.getFlags());
        remap[startEntity.getID()] = cloned.getID();

        for (const Component* pComp : startEntity.getAllComponents()) {
            if (!pComp->isSerializable()) {
                continue;
            }

            cloned.addComponent(pComp->duplicate(cloned));
        }

        cloned.addComponent<PrefabInstanceComponent>();
        PrefabInstanceComponent& component = cloned.getComponent<PrefabInstanceComponent>();
        component.prefab = shared_from_this();
        component.childID = startEntity.getID();

        for (const auto& child : startEntity.getChildren(ShouldRecurse::NoRecursion)) {
            Entity clonedChild = instantiateSubTree(world, child.getID(), remap);
            clonedChild.setParent(cloned);
        }
        return cloned;
    }

    /// Instantiate a copy of this prefab in the given world
    Entity Prefab::instantiate(World& world) const {
        verify(!path.isEmpty(), "Cannot instantiate a prefab which has no path (required because added to entities for edition & serialisation)");
        std::unordered_map<Carrot::ECS::EntityID, Carrot::ECS::EntityID> remap;

        Entity rootEntity = world.newEntity(path.getPath().getFilename());
        rootEntity.addComponent<PrefabInstanceComponent>();
        rootEntity.addComponent<TransformComponent>();
        PrefabInstanceComponent& component = rootEntity.getComponent<PrefabInstanceComponent>();
        component.prefab = shared_from_this();
        component.childID = PrefabRootUUID;

        for (const auto& e : internalScene.world.getAllEntities()) {
            Entity cloned = world.newEntity(e.getName());
            cloned.setFlags(e.getFlags());
            remap[e.getID()] = cloned.getID();

            for (const Component* pComp : e.getAllComponents()) {
                if (!pComp->isSerializable()) {
                    continue;
                }

                cloned.addComponent(pComp->duplicate(cloned));
            }

            cloned.addComponent<PrefabInstanceComponent>();
            PrefabInstanceComponent& component = cloned.getComponent<PrefabInstanceComponent>();
            component.prefab = shared_from_this();
            component.childID = e.getID();
        }
        for (const auto& e : internalScene.world.getAllEntities()) {
            auto optParent = e.getParent();
            if (optParent.has_value()) {
                world.wrap(remap[e.getID()]).setParent(world.wrap(remap[optParent.value().getID()]));
            } else {
                world.wrap(remap[e.getID()]).setParent(rootEntity);
            }
        }

        world.repairLinks(rootEntity, remap); // apply prefab->clone mapping to components referencing entities
        return rootEntity;
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