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
#include <engine/ecs/components/ErrorComponent.h>
#include <engine/ecs/components/MissingComponent.h>

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
    static Carrot::DocumentElement serialiseWithoutDefaultValues(ECS::Component& instanceComponent, ECS::Component& prefabComponent, bool& outputAnything) {
        auto rootInstanceJSON = instanceComponent.serialise();
        auto rootPrefabJSON = prefabComponent.serialise();
        Carrot::DocumentElement result;

        // returns true iif a member was added to 'output' (to skip over nested objects which have the same value as the prefab)
        std::function<bool(const Carrot::DocumentElement&, const Carrot::DocumentElement&, Carrot::DocumentElement&)>
        diff = [&diff](const Carrot::DocumentElement& instanceData, const Carrot::DocumentElement& prefabData, Carrot::DocumentElement& output) -> bool {
            bool addedToOutput = false;
            auto instanceAsObject = instanceData.getAsObject();
            auto prefabAsObject = prefabData.getAsObject();
            // go over each member, and remove it if it has the same value as the prefab (this becomes recursive for objects)
            for(auto it = instanceAsObject.begin(); it != instanceAsObject.end(); ++it) {
                auto prefabIter = prefabAsObject.find(it->first);

                auto addNoModification = [&]() {
                    addedToOutput = true;
                    output[it->first] = it->second;
                };
                if(prefabIter == prefabAsObject.end()) {
                    addNoModification();
                    continue; // no such value in prefab, keep it
                }

                auto& instanceMember = it->second;
                auto& prefabMember = prefabIter->second;
                if(instanceMember.isObject() && prefabMember.isObject()) {
                    // deep modification
                    Carrot::DocumentElement resultMember;
                    bool different = diff(instanceMember, prefabMember, resultMember);
                    if(different) {
                        output[it->first] = resultMember;
                        addedToOutput = true;
                    }
                } else if(instanceMember != prefabMember) {
                    addNoModification();
                } // else: same value, don't add to instance
            }
            return addedToOutput;
        };

        outputAnything = diff(rootInstanceJSON, rootPrefabJSON, result);
        return result;
    }

    // Fill in default values missing from instanceData, based on prefabComponent (Inverse of serialiseWithoutDefaultValues)
    static Carrot::DocumentElement deserialiseWithDefaultValues(ECS::Component& prefabComponent, const Carrot::DocumentElement& instanceData) {
        auto prefabJSON = prefabComponent.serialise();

        // start by copying values of instance
        Carrot::DocumentElement result = instanceData;

        std::function<void(const Carrot::DocumentElement&, Carrot::DocumentElement&)>
        diff = [&diff](const Carrot::DocumentElement& prefabData, Carrot::DocumentElement& result) {
            auto objectView = prefabData.getAsObject();

            // go over all members of prefab JSON, to see if it is missing from the result object
            for(auto prefabIt = objectView.begin(); prefabIt != objectView.end(); ++prefabIt) {
                const auto& name = prefabIt->first;
                auto resultAsObject = result.getAsObject();
                auto it = resultAsObject.find(name);
                const auto& prefabValue = prefabIt->second;

                if(it != resultAsObject.end()) { // instance already has a value for this entry, might need to merge contents of prefab
                    auto& instanceValue = result[name];
                    if(instanceValue.isObject() && prefabValue.isObject()) {
                        // recursive
                        diff(prefabValue, instanceValue);
                    }
                    // if not objects, leave it as-is (instance has an override)
                } else { // instance does not have a value for this entry, add it
                    result[name] = prefabValue;
                }
            }
        };
        diff(prefabJSON, result);
        return result;
    }

    bool Scene::isValidSceneFolder(const Carrot::IO::VFS::Path& sceneFolder) {
        auto& vfs = GetVFS();
        if (!vfs.isDirectory(sceneFolder)) {
            return false;
        }

        return vfs.exists(sceneFolder / "WorldData.toml");
    }

    void Scene::serialise(const std::filesystem::path& sceneFolder) const {
        namespace fs = std::filesystem;

        fs::path backupFolder = sceneFolder;
        backupFolder.replace_extension("backup");
        fs::create_directories(backupFolder);
        if (fs::exists(sceneFolder)) {
            fs::copy(sceneFolder, backupFolder, fs::copy_options::recursive | fs::copy_options::overwrite_existing);

            fs::remove_all(sceneFolder);
        } else {
            fs::create_directories(sceneFolder);
        }
        // save entity data
        std::function<void(const ECS::Entity& entity, const fs::path& entityFolder)> saveEntityTree = [&](ECS::Entity entity, const fs::path& entityFolder) {
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

            auto prefabInstanceComponent = entity.getComponent<ECS::PrefabInstanceComponent>();
            std::shared_ptr<const ECS::Prefab> pPrefab = prefabInstanceComponent.hasValue() ? prefabInstanceComponent.asPtr()->prefab : nullptr;

            // TODO: check if prefab entity
            for (const auto& pComp : entity.getAllComponents()) {
                if (!pComp->isSerializable()) {
                    continue;
                }

                toml::table tomlComp;

                if(pPrefab != nullptr && pComp->getComponentTypeID() != ECS::PrefabInstanceComponent::getID()) {
                    auto optComponentRef = pPrefab->getComponent(prefabInstanceComponent->childID, pComp->getComponentTypeID());
                    if (optComponentRef.hasValue()) {
                        ECS::Component& prefabComponent = optComponentRef;
                        bool outputAnything = false;
                        auto result = serialiseWithoutDefaultValues(*pComp, prefabComponent, outputAnything);
                        if(outputAnything) {
                            tomlComp << result;
                        }
                    } else { // it is valid to add components to prefab instances which are not already inside the prefab
                        tomlComp << pComp->serialise();
                    }
                } else {
                    // prefab instance components are used for serialisation, don't modify them
                    // or if there is no prefab, no need to modify anything

                    tomlComp << pComp->serialise();
                }
                std::string filename { pComp->getName() };
                filename += ".toml";
                std::ofstream f { entityFolder / filename, std::ios::binary };
                f << tomlComp;
            }

            for (const auto& child : world.getChildren(entity, ShouldRecurse::NoRecursion)) {
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
            std::unordered_map<Carrot::ECS::EntityID, Carrot::ECS::EntityID> remap;
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

                    // start by checking if this entity is a prefab instance, because this impacts how deserialisation will work
                    std::shared_ptr<const ECS::Prefab> pPrefab;
                    Carrot::UUID prefabChildID = Carrot::UUID::null();
                    std::string prefabInstanceFilename { ECS::PrefabInstanceComponent::getStringRepresentation() };
                    prefabInstanceFilename += ".toml";

                    std::optional<std::unordered_set<ECS::EntityID>> expectedPrefabChildren;
                    if (vfs.exists(entityFolder / prefabInstanceFilename)) {
                        const auto& prefabInstanceData = loadDocumentFromVFS(entityFolder / prefabInstanceFilename);
                        auto component = componentLib.deserialise(ECS::PrefabInstanceComponent::getStringRepresentation(), prefabInstanceData, self);
                        self.addComponent(std::move(component));

                        auto prefabInstanceComp = self.getComponent<ECS::PrefabInstanceComponent>();
                        pPrefab = prefabInstanceComp->prefab;
                        prefabChildID = prefabInstanceComp->childID;

                        if(pPrefab) {
                            // add all components which are not saved inside instance, because they are exactly the same as the prefab's
                            for(const ECS::Component* pComponent : pPrefab->getAllComponents(prefabChildID)) {
                                std::string filename = pComponent->getName();
                                filename += ".toml";

                                if(!vfs.exists(entityFolder / filename)) { // no instance overrides, copy prefab's component
                                    self.addComponent(pComponent->duplicate(self));
                                }
                            }

                            expectedPrefabChildren = pPrefab->getChildrenIDs(prefabChildID);
                        }
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

                            std::string componentName { childPath.getPath().getStem() };
                            if(componentName == ECS::PrefabInstanceComponent::getStringRepresentation()) {
                                continue;
                            }
                            Carrot::DocumentElement doc = loadDocumentFromVFS(childPath);
                            if (!componentLib.has(componentName)) {
                                self.addComponent(std::make_unique<ECS::MissingComponent>(self, componentName, doc));
                                continue;
                            }
                            if(pPrefab != nullptr) {
                                if (!pPrefab->hasChildWithID(prefabChildID)) {
                                    auto pErrorComponent = std::make_unique<ECS::ErrorComponent>(self);
                                    pErrorComponent->message = Carrot::sprintf("Prefab '%s' has no child with UUID %s", pPrefab->getVFSPath().toString().c_str(), prefabChildID.toString().c_str());
                                    self.addComponent(std::move(pErrorComponent));
                                } else {
                                    Memory::OptionalRef<Carrot::ECS::Component> prefabComponent = pPrefab->getComponentByName(prefabChildID, componentName);
                                    if(prefabComponent.hasValue()) {
                                        // there is a prefab for this entity, and the prefab has the component, fill in default values if missing:
                                        auto component = componentLib.deserialise(
                                            componentName,
                                            deserialiseWithDefaultValues(prefabComponent.asRef(), doc),
                                            self);
                                        self.addComponent(std::move(component));
                                    } else {
                                        // not part of prefab, load directly
                                        auto component = componentLib.deserialise(componentName, doc, self);
                                        self.addComponent(std::move(component));
                                    }
                                }
                            } else {
                                // no prefab for this entity, load directly
                                auto component = componentLib.deserialise(componentName, doc, self);
                                self.addComponent(std::move(component));
                            }
                        }
                    }

                    if (expectedPrefabChildren.has_value()) {
                        // remove children that no longer exist
                        for (auto& child : self.getChildren(ShouldRecurse::NoRecursion)) {
                            auto prefabRef = child.getComponent<ECS::PrefabInstanceComponent>();
                            // prefab instances are not forbidden to add children to the hierarchy of the instance which were not present in original prefab
                            if (!prefabRef.hasValue()) {
                                continue;
                            }

                            if (!expectedPrefabChildren->contains(prefabRef->childID)) {
                                child.remove();
                            } else {
                                expectedPrefabChildren->erase(prefabRef->childID);
                            }
                        }

                        // add missing children
                        for (auto& childID : expectedPrefabChildren.value()) {
                            ECS::Entity subtree = pPrefab->instantiateSubTree(world, childID, remap);
                            subtree.setParent(self);
                        }
                    }

                    return self;
                };

                ECS::Entity rootEntity = loadEntity(path);
                DISCARD(rootEntity);
            }
            if (!remap.empty()) {
                world.repairLinks(remap);
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
