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
    Prefab::Prefab(const Carrot::IO::VFS::Path& prefabAsset): path(prefabAsset) {
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
            std::unique_ptr<Component> prefabComponent = Carrot::ECS::getComponentLibrary().deserialise(name, it->value, fakeEntity);
            components[prefabComponent->getComponentTypeID()] = std::move(prefabComponent);
        }

        auto childrenObject = doc["children"].GetArray();
        for(std::size_t i = 0; i < childrenObject.Size(); i++) {
            const Carrot::IO::VFS::Path childPath { std::string_view { childrenObject[i].GetString(), childrenObject[i].GetStringLength() } };
            std::unique_ptr<Prefab> child = std::make_unique<Prefab>(childPath);
            children[child->getEntityID()] = std::move(child);
        }
    }

    /// Gets the given component inside this prefab, if it exists
    Memory::OptionalRef<Component> Prefab::getComponent(const ComponentID& componentID) const {
        TODO;
    }

    /// Gets the given component inside this prefab, or creates it if it does not exist
    Component& Prefab::getOrAddComponent(const ComponentID& componentID) {
        TODO;
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

        return true;
    }

    /// Initializes this prefab with a copy of the contents of the given entity.
    void Prefab::createFromEntity(const Carrot::ECS::Entity& entity) {
        name = entity.getName();
        fakeEntityID = entity.getID();
        components.clear();
        for(const Carrot::ECS::Component* pComponent : entity.getAllComponents()) {
            components[pComponent->getComponentTypeID()] = pComponent->duplicate(fakeEntity);
        }

        for(const auto& child : entity.getWorld().getChildren(entity, ShouldRecurse::NoRecursion)) {
            std::unique_ptr<Prefab> childPrefab = std::make_unique<Prefab>();
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
            component.prefabPath = prefab.path;
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
} // Carrot::ECS