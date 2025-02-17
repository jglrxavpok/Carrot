//
// Created by jglrxavpok on 06/10/2021.
//

#include "Scene.h"

#include <core/io/Document.h>
#include <core/io/DocumentHelpers.h>
#include <engine/ecs/Prefab.h>
#include <engine/ecs/components/PrefabInstanceComponent.h>

#include "core/utils/JSON.h"
#include "core/io/Logging.hpp"
#include "engine/render/RenderContext.h"
#include "engine/utils/Macros.h"
#include "engine/Engine.h"
#include "engine/render/VulkanRenderer.h"
#include <core/utils/TOML.h>

namespace Carrot {
    Scene::~Scene() {
        for(auto& pViewport : viewports) {
            unbindFromViewport(*pViewport);
        }
    }

    void Scene::tick(double frameTime) {
        world.tick(frameTime);
    }

    void Scene::prePhysics() {
        world.prePhysics();
    }

    void Scene::postPhysics() {
        world.postPhysics();
    }

    void Scene::setupCamera(const Carrot::Render::Context& renderContext) {
        world.setupCamera(renderContext);
    }

    void Scene::onFrame(const Carrot::Render::Context& renderContext) {
        world.onFrame(renderContext);
    }

    void Scene::clear() {
        copyFrom(Scene{});
    }

    // Serialises instanceComponent, but removes all values which are the same than prefabComponent. These will be filled at load time when deserialisation happens
    static rapidjson::Value serialiseWithoutDefaultValues(ECS::Component& instanceComponent, ECS::Component& prefabComponent, rapidjson::Document& document /* TODO: use allocator instead */, bool& outputAnything) {
        TODO;
        /*
        auto rootInstanceJSON = instanceComponent.toJSON(document);
        auto rootPrefabJSON = prefabComponent.toJSON(document);
        rapidjson::Value result { rapidjson::kObjectType };

        // returns true iif a member was added to 'output' (to skip over nested objects which have the same value as the prefab)
        std::function<bool(const rapidjson::Value& instanceJSON, const rapidjson::Value& prefabJSON, rapidjson::Value& output, rapidjson::Document::AllocatorType& allocator)>
        diff = [&diff](const rapidjson::Value& instanceJSON, const rapidjson::Value& prefabJSON, rapidjson::Value& output, rapidjson::Document::AllocatorType& allocator) -> bool {
            auto copy = [&](const rapidjson::Value& v) {
                return rapidjson::Value{ v, allocator };
            };
            bool addedToOutput = false;
            // go over each member, and remove it if it has the same value as the prefab (this becomes recursive for objects)
            for(auto it = instanceJSON.MemberBegin(); it != instanceJSON.MemberEnd(); ++it) {
                auto prefabIter = prefabJSON.FindMember(it->name);

                auto addNoModification = [&]() {
                    addedToOutput = true;
                    output.AddMember(copy(it->name), copy(it->value), allocator);
                };
                if(prefabIter == prefabJSON.MemberEnd()) {
                    addNoModification();
                    continue; // no such value in prefab, keep it
                }

                auto& instanceMember = it->value;
                auto& prefabMember = prefabIter->value;
                if(instanceMember.IsObject() && prefabMember.IsObject()) {
                    // deep modification
                    rapidjson::Value resultMember { rapidjson::kObjectType };
                    bool different = diff(instanceMember, prefabMember, resultMember, allocator);
                    if(different) {
                        output.AddMember(copy(it->name), resultMember, allocator);
                        addedToOutput = true;
                    }
                } else if(instanceMember != prefabMember) {
                    addNoModification();
                } // else: same value, don't add to instance
            }
            return addedToOutput;
        };

        outputAnything = diff(rootInstanceJSON, rootPrefabJSON, result, document.GetAllocator());
        return result;*/
        return {};
    }

    // Fill in default values missing from instanceJSON, based on prefabComponent (Inverse of serialiseWithoutDefaultValues)
    static rapidjson::Value deserialiseWithDefaultValues(ECS::Component& prefabComponent, const rapidjson::Value& instanceJSON, rapidjson::Document& fakeDoc) {
        TODO;
        /*
        auto& allocator = fakeDoc.GetAllocator();
        auto prefabJSON = prefabComponent.toJSON(fakeDoc);

        // start by copying values of instance
        rapidjson::Value result { instanceJSON, allocator };

        std::function<void(const rapidjson::Value& prefabJSON, rapidjson::Value& result, rapidjson::Document::AllocatorType& allocator)>
        diff = [&diff](const rapidjson::Value& prefabJSON, rapidjson::Value& result, rapidjson::Document::AllocatorType& allocator) {
            auto copy = [&](const rapidjson::Value& v) {
                return rapidjson::Value{ v, allocator };
            };

            // go over all members of prefab JSON, to see if it is missing from the result object
            for(auto prefabIt = prefabJSON.MemberBegin(); prefabIt != prefabJSON.MemberEnd(); ++prefabIt) {
                const auto& name = prefabIt->name;
                auto it = result.FindMember(name);
                auto& prefabValue = it->value;

                if(it != result.MemberEnd()) { // instance already has a value for this entry, might need to merge contents of prefab
                    auto& instanceValue = it->value;
                    if(instanceValue.IsObject() && prefabValue.IsObject()) {
                        // recursive
                        diff(prefabValue, instanceValue, allocator);
                    }
                    // if not objects, leave it as-is (instance has an override)
                } else { // instance does not have a value for this entry, add it
                    result.AddMember(copy(name), copy(prefabIt->value), allocator);
                }
            }
        };
        diff(prefabJSON, result, allocator);
        return result;*/
        return {};
    }

    void Scene::serialise(const std::filesystem::path& sceneFolder) const {
        namespace fs = std::filesystem;

        fs::path backupFolder = sceneFolder;
        backupFolder.replace_extension("backup");
        fs::create_directories(backupFolder);
        fs::copy(sceneFolder, backupFolder, fs::copy_options::recursive | fs::copy_options::overwrite_existing);

        fs::remove_all(sceneFolder);
        // save entity data
        std::function<void(const ECS::Entity& entity, const fs::path& entityFolder)> saveEntityTree = [&](const ECS::Entity& entity, const fs::path& entityFolder) {
            fs::create_directories(entityFolder);
            {
                Carrot::IO::FileHandle f { entityFolder / ".uuid", IO::OpenMode::NewOrExistingReadWrite };
                std::string uuidStr = entity.getID().toString();
                f.write(std::span{ reinterpret_cast<u8*>(uuidStr.data()), uuidStr.size() });
            }
            if (entity.getFlags() != 0) {
                Carrot::IO::FileHandle f { entityFolder / ".flags", IO::OpenMode::NewOrExistingReadWrite };
                std::string flagsStr = Carrot::ECS::flagsToString(entity.getFlags());
                f.write(std::span{ reinterpret_cast<u8*>(flagsStr.data()), flagsStr.size() });
            }

            // TODO: check if prefab entity
            for (const auto& pComp : entity.getAllComponents()) {
                toml::table tomlComp;
                tomlComp << pComp->serialise();
                std::string filename { pComp->getName() };
                filename += ".toml";
                std::ofstream f { entityFolder / filename, std::ios::binary };
                f << tomlComp;
            }

            for (const auto& child : world.getChildren(entity)) {
                saveEntityTree(child, entityFolder / child.getName());
            }
        };

        for (const auto& entity : world.getAllEntities()) {
            if (entity.getParent().has_value()) {
                continue;
            }

            saveEntityTree(entity, sceneFolder / entity.getName());
        }

        // save 'global' data
        fs::create_directories(sceneFolder / ".LogicSystems");
        for (const auto& pSystem : world.getLogicSystems()) {
            if (!pSystem->shouldBeSerialized()) {
                continue;
            }
            toml::table tomlComp;
            tomlComp << pSystem->serialise();
            std::string filename { pSystem->getName() };
            filename += ".toml";
            std::ofstream f { sceneFolder / ".LogicSystems" / filename, std::ios::binary };
            f << tomlComp;
        }

        fs::create_directories(sceneFolder / ".RenderSystems");
        for (const auto& pSystem : world.getRenderSystems()) {
            if (!pSystem->shouldBeSerialized()) {
                continue;
            }
            toml::table tomlComp;
            tomlComp << pSystem->serialise();
            std::string filename { pSystem->getName() };
            filename += ".toml";
            std::ofstream { sceneFolder / ".RenderSystems" / filename, std::ios::binary } << tomlComp;
        }


        {
            toml::table tomlComp;
            tomlComp << world.getWorldData().serialise();
            std::ofstream { sceneFolder / "WorldData.toml", std::ios::binary } << tomlComp;
        }
        {
            toml::table tomlComp;
            Carrot::DocumentElement doc;
            doc["name"] = Carrot::Skybox::getName(skybox);
            tomlComp << doc;
            std::ofstream { sceneFolder / "Skybox.toml", std::ios::binary } << tomlComp;
        }
        {
            toml::table tomlComp;
            Carrot::DocumentElement doc;
            doc["raytracedShadows"] = lighting.raytracedShadows;
            doc["ambient"] = Carrot::DocumentHelpers::write(lighting.ambient);
            tomlComp << doc;
            std::ofstream { sceneFolder / "Lighting.toml", std::ios::binary } << tomlComp;
        }

        // remove backup since saving went well
        fs::remove_all(backupFolder);
#if 0
        rapidjson::Value entitiesMap(rapidjson::kObjectType);
        for(auto& entity : world.getAllEntities()) {
            rapidjson::Value entityData(rapidjson::kObjectType);
            entityData.SetObject();
            auto components = world.getAllComponents(entity);

            auto prefabCompRef = entity.getComponent<ECS::PrefabInstanceComponent>();
            std::shared_ptr<const ECS::Prefab> pPrefab = prefabCompRef.hasValue() ? prefabCompRef->prefab : nullptr;

            for(const auto& comp : components) {
                if(!comp->isSerializable()) {
                    continue;
                }
                rapidjson::Value key(comp->getName(), dest.GetAllocator());
                if(pPrefab != nullptr && comp->getComponentTypeID() != ECS::PrefabInstanceComponent::getID()) {
                    auto optComponentRef = pPrefab->getComponent(comp->getComponentTypeID());
                    if (optComponentRef.hasValue()) {
                        ECS::Component& prefabComponent = optComponentRef;
                        bool outputAnything = false;
                        auto result = serialiseWithoutDefaultValues(*comp, prefabComponent, dest, outputAnything);
                        if(outputAnything) {
                            entityData.AddMember(key, result, dest.GetAllocator());
                        }
                    } else { // it is valid to add components to prefab instances which are not already inside the prefab
                        entityData.AddMember(key, comp->toJSON(dest), dest.GetAllocator());
                    }
                } else {
                    // prefab instance components are used for serialisation, don't modify them
                    // or if there is no prefab, no need to modify anything

                    entityData.AddMember(key, comp->toJSON(dest), dest.GetAllocator());
                }
            }
#endif
    }

    void Scene::deserialise(const Carrot::IO::VFS::Path& sceneFolder) {
        try { // TODO: remove try catch
            auto& vfs = GetVFS();
            auto& componentLib = Carrot::ECS::getComponentLibrary();
            auto& systemLib = Carrot::ECS::getSystemLibrary();

            rapidjson::Document fakeDoc; // for its allocator

            auto loadDocumentFromVFS = [&](const Carrot::IO::VFS::Path& p) {
                Carrot::DocumentElement doc;
                Carrot::IO::Resource r { p };
                toml::table toml = toml::parse(r.readText());
                toml >> doc;
                return doc;
            };
            auto loadDocument = [&](const char* relativePath) {
                return loadDocumentFromVFS(sceneFolder / relativePath);
            };

            // load first, that way entities can refer to shared data
            if(vfs.exists(sceneFolder / "WorldData.toml")) {
                ECS::WorldData& worldData = world.getWorldData();
                worldData.deserialise(loadDocument("WorldData.toml"));
            }

            if(vfs.exists(sceneFolder / "Lighting.toml")) {
                auto src = loadDocument("Lighting.toml");
                lighting.ambient = Carrot::DocumentHelpers::read<3, float>(src["ambient"]);
                lighting.raytracedShadows = src["raytracedShadows"].getAsBool();
            }

            if(vfs.exists(sceneFolder / "Skybox.toml")) {
                auto src = loadDocument("Skybox.toml");
                std::string skyboxStr { src["name"].getAsString() };
                if(!Carrot::Skybox::safeFromName(skyboxStr, skybox)) {
                    Carrot::Log::error("Unknown skybox: %s", skyboxStr.c_str());
                }
            }

            // load entities
            for (const auto path : vfs.iterateOverDirectory(sceneFolder)) {
                if (!vfs.isDirectory(path)) {
                    continue;
                }

                std::string_view entityName = path.getPath().getFilename();
                if (ECS::isIllegalEntityName(entityName)) {
                    continue;
                }

                std::function<ECS::Entity(const IO::VFS::Path& entityFolder)> loadEntity = [&](const IO::VFS::Path& entityFolder) -> ECS::Entity {
                    std::string_view selfName = entityFolder.getPath().getFilename();
                    if (!vfs.exists(entityFolder / ".uuid")) {
                        Log::error("Folder '%s' has no .uuid file, not a valid entity", entityFolder.toString().c_str());
                        return {};
                    }

                    UUID selfID = UUID::fromString(IO::Resource { entityFolder / ".uuid" }.readText());
                    ECS::Entity self = world.newEntityWithID(selfID, selfName);

                    if (vfs.exists(entityFolder / ".flags")) {
                        const ECS::EntityFlags flags = ECS::stringToFlags(IO::Resource{ entityFolder / ".flags"}.readText());
                        self.setFlags(flags);
                    }

                    for (const auto childPath : vfs.iterateOverDirectory(entityFolder)) {
                        // child entity
                        if (vfs.isDirectory(childPath)) {
                            std::string_view childName = childPath.getPath().getFilename();
                            if (ECS::isIllegalEntityName(childName)) {
                                continue;
                            }

                            ECS::Entity child = loadEntity(childPath);
                            child.setParent(self);
                        } else {
                            // potentially a component
                            if (childPath.getExtension() != ".toml") {
                                continue;
                            }

                            // TODO: prefabs
                            Carrot::DocumentElement doc = loadDocumentFromVFS(childPath);
                            std::string componentName { childPath.getPath().getStem() };
                            auto pComp = componentLib.deserialise(componentName, doc, self);
                            self.addComponent(std::move(pComp));
                        }
                    }

                    return self;
                };

                ECS::Entity rootEntity = loadEntity(path);
                DISCARD(rootEntity);
            }

            for (const auto systemPath : vfs.iterateOverDirectory(sceneFolder / ".RenderSystems")) {
                const std::string systemName { systemPath.getPath().getStem() };
                if (systemLib.has(systemName)) {
                    auto system = systemLib.deserialise(systemName, loadDocumentFromVFS(systemPath), world);
                    world.addRenderSystem(std::move(system));
                } else {
                    TODO; // dummy system
                }
            }

            for (const auto systemPath : vfs.iterateOverDirectory(sceneFolder / ".LogicSystems")) {
                const std::string systemName { systemPath.getPath().getStem() };
                if (systemLib.has(systemName)) {
                    auto system = systemLib.deserialise(systemName, loadDocumentFromVFS(systemPath), world);
                    world.addLogicSystem(std::move(system));
                } else {
                    TODO; // dummy system
                }
            }
        } catch (std::exception& e) {
            Carrot::Log::error("Failed to deserialise scene: %s", e.what());
            throw;
        } catch (...) {
            Carrot::Log::error("Failed to deserialise scene!!");
            throw;
        }
#if 0

            // start by checking if this entity is a prefab instance, because this impacts how deserialisation will work
            std::shared_ptr<const ECS::Prefab> pPrefab;
            const auto& prefabInstanceJSON = data.FindMember(ECS::PrefabInstanceComponent::getStringRepresentation());
            if(prefabInstanceJSON != data.MemberEnd()) {
                auto component = componentLib.deserialise(ECS::PrefabInstanceComponent::getStringRepresentation(), prefabInstanceJSON->value, entity);
                entity.addComponent(std::move(component));

                pPrefab = entity.getComponent<ECS::PrefabInstanceComponent>()->prefab;

                if(pPrefab) {
                    // add all components which are not saved inside instance, because they are exactly the same as the prefab's
                    for(const ECS::Component* pComponent : pPrefab->getAllComponents()) {
                        if(!data.HasMember(pComponent->getName())) { // no instance overrides, copy prefab's component
                            entity.addComponent(pComponent->duplicate(entity));
                        }
                    }
                }
            }

            for(const auto& [componentNameKey, componentDataKey] : data) {
                std::string componentName = componentNameKey.GetString();
                if(componentName == "name" || componentName == "parent" || componentName == "flags" || componentName == ECS::PrefabInstanceComponent::getStringRepresentation()) {
                    continue;
                }
                if(pPrefab != nullptr) {
                    Memory::OptionalRef<Carrot::ECS::Component> prefabComponent = pPrefab->getComponentByName(componentName);
                    if(prefabComponent.hasValue()) {
                        // there is a prefab for this entity, and the prefab has the component, fill in default values if missing:
                        auto component = componentLib.deserialise(
                            componentName,
                            deserialiseWithDefaultValues(prefabComponent.asRef(), componentDataKey, fakeDoc),
                            entity);
                        entity.addComponent(std::move(component));
                    } else {
                        // not part of prefab, load directly
                        auto component = componentLib.deserialise(componentName, componentDataKey, entity);
                        entity.addComponent(std::move(component));
                    }
                } else {
                    // no prefab for this entity, load directly
                    auto component = componentLib.deserialise(componentName, componentDataKey, entity);
                    entity.addComponent(std::move(component));
                }
            }
        }

#endif
    }

    void Scene::load() {
        world.reloadSystems();

        GetRenderer().getLighting().getAmbientLight() = lighting.ambient;
        GetEngine().setSkybox(skybox);
    }

    void Scene::unload() {
        world.unloadSystems();
    }

    void Scene::bindToViewport(Carrot::Render::Viewport& viewport) {
        viewport.addScene(this);
        viewports.emplace_back(&viewport);
    }

    void Scene::unbindFromViewport(Carrot::Render::Viewport& viewport) {
        viewport.removeScene(this);
        std::erase(viewports, &viewport);
    }

    std::span<Carrot::Render::Viewport*> Scene::getViewports() {
        return viewports;
    }

    void Scene::copyFrom(const Scene& toCopy) {
        world = toCopy.world;
        lighting = toCopy.lighting;
        skybox = toCopy.skybox;
    }
}