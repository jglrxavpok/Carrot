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
        const Carrot::IO::Resource r { prefabAsset };
        std::string text = r.readText();

        rapidjson::Document doc;
        doc.Parse(text);

        auto& jsonName = doc["name"];
        setName(std::string_view { jsonName.GetString(), jsonName.GetStringLength() });

        auto& jsonEntityID = doc["entity_id"];
        setEntityID(Carrot::ECS::EntityID{ std::string { std::string_view { jsonEntityID.GetString(), jsonEntityID.GetStringLength() } } });

        auto& componentsObject = doc["components"];
        for(auto it = componentsObject.MemberBegin(); it != componentsObject.MemberEnd(); ++it) {
            std::string name { std::string_view { it->name.GetString() , it->name.GetStringLength() } };
            deserialiseAddComponent(name, it->value);
        }

        auto childrenObject = doc["children"].GetArray();
        for(std::size_t i = 0; i < childrenObject.Size(); i++) {
            const Carrot::IO::VFS::Path childPath { std::string_view { childrenObject[i].GetString(), childrenObject[i].GetStringLength() } };
            std::shared_ptr<Prefab> child = GetAssetServer().loadPrefab(task, childPath);
            children[child->getEntityID()] = std::move(child);
        }
    }

    /// Gets the given component inside this prefab, if it exists
    Memory::OptionalRef<Component> Prefab::getComponent(const ComponentID& componentID) const {
        auto iter = components.find(componentID);
        if(iter == components.end()) {
            return {};
        }
        return *iter->second;
    }

    Memory::OptionalRef<Component> Prefab::getComponentByName(std::string_view componentName) const {
        for(const auto& [_, pComp] : components) {
            if(componentName == pComp->getName()) {
                return *pComp;
            }
        }
        return {};
    }

    /// Creates and adds a new component for this prefab
    Component& Prefab::addComponent(std::string_view componentName) {
        auto comp = Carrot::ECS::getComponentLibrary().create(std::string { componentName }, fakeEntity);
        Carrot::ComponentID compID = comp->getComponentTypeID();
        components[compID] = std::move(comp);
        return *components[compID];
    }

    Component& Prefab::deserialiseAddComponent(std::string_view componentName, const rapidjson::Value& json) {
        std::unique_ptr<Component> prefabComponent = Carrot::ECS::getComponentLibrary().deserialise(std::string { componentName }, json, fakeEntity);
        Carrot::ComponentID compID = prefabComponent->getComponentTypeID();
        components[compID] = std::move(prefabComponent);
        return *components[compID];
    }

    /// Removes a component from this prefab.
    /// Returns false if the component was not present inside this prefab
    bool Prefab::removeComponent(const ComponentID& compID) {
        return components.erase(compID) != 0;
    }

    Vector<const Component*> Prefab::getAllComponents() const {
        Vector<const Component*> output;
        output.ensureReserve(components.size());
        for(const auto& [_, pComp] : components) {
            output.emplaceBack(pComp.get());
        }
        return output;
    }


    /// Save to disk. Can fail if the target path is not writtable.
    bool Prefab::save(const Carrot::IO::VFS::Path& prefabAsset) {
        this->path = prefabAsset;
        auto diskPath = GetVFS().resolve(prefabAsset);
        Carrot::IO::FileHandle f { diskPath, IO::OpenMode::NewOrExistingReadWrite };

        rapidjson::Document document { rapidjson::kObjectType };
        auto& allocator = document.GetAllocator();

        rapidjson::Value componentsObject { rapidjson::kObjectType };
        for(const auto& [compID, pComponent] : components) {
            componentsObject.AddMember(rapidjson::Value { pComponent->getName(), allocator }, pComponent->toJSON(document), allocator);
        }
        document.AddMember("components", componentsObject, allocator);
        document.AddMember("name", rapidjson::Value { name.c_str(), allocator }, allocator);
        document.AddMember("entity_id", rapidjson::Value { fakeEntityID.toString().c_str(), allocator }, allocator);

        diskPath += ".children";
        rapidjson::Value childrenArray { rapidjson::kArrayType };
        for(const auto& [childID, pChild] : children) {
            std::filesystem::path childDiskPath = diskPath / childID.toString();
            childDiskPath += ".cprefab";
            if(!std::filesystem::exists(childDiskPath.parent_path())) {
                std::filesystem::create_directories(childDiskPath.parent_path());
            }

            const Carrot::IO::VFS::Path childVFSPath = GetVFS().represent(childDiskPath).value();
            bool savedChild = pChild->save(childVFSPath);
            if(!savedChild) {
                return false;
            }

            childrenArray.PushBack(rapidjson::Value{ childVFSPath.toString().c_str(), allocator }, allocator);
        }
        document.AddMember("children", childrenArray, allocator);

        rapidjson::StringBuffer strBuffer;
        rapidjson::PrettyWriter writer { strBuffer };
        document.Accept(writer);
        f.write(std::span { reinterpret_cast<const std::uint8_t*>(strBuffer.GetString()), strBuffer.GetLength() });

        GetAssetServer().storePrefab(*this);
        return true;
    }

    /// Initializes this prefab with a copy of the contents of the given entity.
    void Prefab::createFromEntity(const Carrot::ECS::Entity& entity) {
        name = entity.getName();
        fakeEntityID = entity.getID();
        components.clear();
        children.clear();
        for(const Carrot::ECS::Component* pComponent : entity.getAllComponents()) {
            components[pComponent->getComponentTypeID()] = pComponent->duplicate(fakeEntity);
        }

        for(const auto& child : entity.getWorld().getChildren(entity, ShouldRecurse::NoRecursion)) {
            std::shared_ptr<Prefab> childPrefab = Prefab::makePrefab();
            childPrefab->createFromEntity(child);
            children[child.getID()] = std::move(childPrefab);
        }
    }

    Entity Prefab::instantiateOnlyThis(World& world) const {
        Entity instantiated = world.newEntity(name); // create the new entity

        // add the components from this prefab
        for(const auto& [compID, pComponent] : components) {
            instantiated.addComponent(pComponent->duplicate(instantiated));
        }

        return instantiated;
    }


    /// Instantiate a copy of this prefab in the given world
    Entity Prefab::instantiate(World& world) const {
        verify(!path.isEmpty(), "Cannot instantiate a prefab which has no path (required because added to entities for edition & serialisation)");
        std::unordered_map<Carrot::ECS::EntityID, Carrot::ECS::EntityID> remap;

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
        return result;
    }

    /// Sets the name of the prefab once instantiated
    void Prefab::setName(std::string_view str) {
        name = str;
    }

    /// Gets the name of the prefab once instantiated
    std::string_view Prefab::getName() const {
        return name;
    }

    void Prefab::setEntityID(const Carrot::ECS::EntityID& entity) {
        fakeEntityID = entity;
        fakeEntity = { fakeEntityID, *(World*)nullptr /* don't do this at home kids */};
    }

    Carrot::ECS::EntityID Prefab::getEntityID() const {
        return fakeEntityID;
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

            action(const_cast<Entity&> /* yolo */ (entity));
        }
    }

} // Carrot::ECS