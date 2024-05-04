//
// Created by jglrxavpok on 29/04/2024.
//

#pragma once

#include <core/io/Resource.h>
#include <engine/ecs/EntityTypes.h>
#include <engine/ecs/components/Component.h>
#include <engine/task/TaskScheduler.h>

namespace Carrot {
    class AssetServer;
}

namespace Carrot::ECS {

/// Prefab: template for entities.
/// This is basically a holder for components, which can:
/// - be instantiated at runtime and in the editor to make instances (clones) of this prefab
/// - be modified inside the editor and apply changes to all instances
///
/// Instances can override values of components to have variation between instances.
/// Any change on a prefab is replicated on its instances:
/// - Adding a component to the prefab will add a copy of the component to instances
/// - Removing a component to the prefab will remove the component from instances too
/// - Modifying a component inside the prefab will apply the same change to instances, if they have not overriden the value
///
/// Overriding means having a different component property value than the prefab. This is determined when:
/// - Loading a scene
/// - Saving a scene
/// - Modifying a prefab
///
/// When stored, entities can reference a prefab, and default values will be omitted from their serialized version when saving.
/// These default values will be used to fill the entity's component upon load.
///
/// Prefabs can have children prefab, which will also be replicated to instances. The instance will therefore have children,
/// which in turn point to the child prefabs of this prefab.
class Prefab: public std::enable_shared_from_this<Prefab> {
public:
    static std::shared_ptr<Prefab> makePrefab();

    /// Gets the given component inside this prefab, if it exists
    Memory::OptionalRef<Component> getComponent(const ComponentID& componentID) const;

    /// Gets the given component inside this prefab, or creates it if it does not exist
    Component& getOrAddComponent(const ComponentID& componentID);

    /// Save to disk. Can fail if the target path is not writtable.
    /// Modifies the VFS path kept internally to refer to this prefab
    bool save(const Carrot::IO::VFS::Path& prefabAsset);

    /// Initializes this prefab with a copy of the contents of the given entity.
    void createFromEntity(const Carrot::ECS::Entity& entity);

    /// Instantiate a copy of this prefab in the given world
    Entity instantiate(World& world) const;

    /// Sets the name of the prefab once instantiated
    void setName(std::string_view str);

    /// Gets the name of the prefab once instantiated
    std::string_view getName() const;

    void setEntityID(const Carrot::ECS::EntityID& entity);

    Carrot::ECS::EntityID getEntityID() const;

    /// VFS path used to create or save this prefab
    const Carrot::IO::VFS::Path& getVFSPath() const;

private:
    Prefab() = default;

    /// Loads this prefab from the given VFS path
    void load(TaskHandle& task, const Carrot::IO::VFS::Path& prefabAsset);

    /// Instantiate a copy of this prefab, EXCLUDING its children.
    /// This is repeatively used to recursively create all instances of the hierarchy, and *then* link everything (see instantiate)
    Entity instantiateOnlyThis(World& world) const;

private:
    Carrot::ECS::EntityID fakeEntityID;
    Carrot::ECS::Entity fakeEntity { fakeEntityID, *(World*)nullptr /* don't do this at home kids */};
    std::string name = "prefab";
    std::unordered_map<ComponentID, std::unique_ptr<Component>> components;
    std::unordered_map<Carrot::UUID, std::shared_ptr<Prefab>> children;

    Carrot::IO::VFS::Path path;

    friend class ::Carrot::AssetServer; // Prefabs are expected to only be loaded from the asset server
};

} // Carrot::ECS
