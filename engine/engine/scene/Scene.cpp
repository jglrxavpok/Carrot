//
// Created by jglrxavpok on 06/10/2021.
//

#include "Scene.h"

#include <engine/ecs/Prefab.h>
#include <engine/ecs/components/PrefabInstanceComponent.h>

#include "core/utils/JSON.h"
#include "core/io/Logging.hpp"
#include "engine/render/RenderContext.h"
#include "engine/utils/Macros.h"
#include "engine/Engine.h"
#include "engine/render/VulkanRenderer.h"

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
        return result;
    }

    // Fill in default values missing from instanceJSON, based on prefabComponent (Inverse of serialiseWithoutDefaultValues)
    static rapidjson::Value deserialiseWithDefaultValues(ECS::Component& prefabComponent, const rapidjson::Value& instanceJSON, rapidjson::Document& fakeDoc) {
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
        return result;
    }

    void Scene::serialise(rapidjson::Document& dest) const {
        dest.AddMember("world_data", world.getWorldData().toJSON(dest.GetAllocator()), dest.GetAllocator());

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
                    ECS::Component& prefabComponent = pPrefab->getComponent(comp->getComponentTypeID());
                    bool outputAnything = false;
                    auto result = serialiseWithoutDefaultValues(*comp, prefabComponent, dest, outputAnything);
                    if(outputAnything) {
                        entityData.AddMember(key, result, dest.GetAllocator());
                    }
                } else {
                    // prefab instance components are used for serialisation, don't modify them
                    // or if there is no prefab, no need to modify anything

                    entityData.AddMember(key, comp->toJSON(dest), dest.GetAllocator());
                }
            }

            if(auto parent = entity.getParent()) {
                rapidjson::Value parentID(parent->getID().toString(), dest.GetAllocator());
                entityData.AddMember("parent", parentID, dest.GetAllocator());
            }

            rapidjson::Value nameKey(std::string(entity.getName()), dest.GetAllocator());
            entityData.AddMember("name", nameKey, dest.GetAllocator());

            const ECS::EntityFlags entityFlags = entity.getFlags();
            if(entityFlags != ECS::EntityFlags::None) {
                rapidjson::Value flagsKey(ECS::flagsToString(entityFlags), dest.GetAllocator());
                entityData.AddMember("flags", flagsKey, dest.GetAllocator());
            }

            rapidjson::Value key(entity.getID().toString(), dest.GetAllocator());
            entitiesMap.AddMember(key, entityData, dest.GetAllocator());
        }

        dest.AddMember("entities", entitiesMap, dest.GetAllocator());

        rapidjson::Value lightingObj(rapidjson::kObjectType);
        {
            lightingObj.AddMember("ambient", Carrot::JSON::write(lighting.ambient, dest), dest.GetAllocator());
            lightingObj.AddMember("raytracedShadows", lighting.raytracedShadows, dest.GetAllocator());
        }

        dest.AddMember("lighting", lightingObj, dest.GetAllocator());
        dest.AddMember("skybox", rapidjson::Value(Carrot::Skybox::getName(skybox), dest.GetAllocator()), dest.GetAllocator());

        rapidjson::Value logicSystems{ rapidjson::kObjectType };
        for(auto& system : world.getLogicSystems()) {
            if(system->shouldBeSerialized()) {
                rapidjson::Value key(system->getName(), dest.GetAllocator());
                logicSystems.AddMember(key, system->toJSON(dest.GetAllocator()), dest.GetAllocator());
            }
        }
        dest.AddMember("logic_systems", logicSystems, dest.GetAllocator());

        rapidjson::Value renderSystems{ rapidjson::kObjectType };
        for(auto& system : world.getRenderSystems()) {
            if(system->shouldBeSerialized()) {
                rapidjson::Value key(system->getName(), dest.GetAllocator());
                renderSystems.AddMember(key, system->toJSON(dest.GetAllocator()), dest.GetAllocator());
            }
        }
        dest.AddMember("render_systems", renderSystems, dest.GetAllocator());
    }

    void Scene::deserialise(const rapidjson::Value& src) {
        assert(src.IsObject());
        rapidjson::Document fakeDoc; // for its allocator

        // load first, that way entities can refer to shared data
        if(src.HasMember("world_data")) {
            ECS::WorldData& worldData = world.getWorldData();
            worldData.loadFromJSON(src["world_data"]);
        }

        const auto entityMap = src["entities"].GetObject();
        auto& componentLib = Carrot::ECS::getComponentLibrary();
        auto& systemLib = Carrot::ECS::getSystemLibrary();
        for(const auto& [key, entityData] : entityMap) {
            Carrot::UUID uuid { key.GetString() };
            auto data = entityData.GetObject();
            std::string name = data["name"].GetString();
            auto entity = world.newEntityWithID(uuid, name);

            if(data.HasMember("parent")) {
                Carrot::UUID parent = data["parent"].GetString();
                entity.setParent(world.wrap(parent));
            }
            if(data.HasMember("flags")) {
                const auto& flagsJson = data["flags"];
                const ECS::EntityFlags flags = ECS::stringToFlags(std::string(flagsJson.GetString(), flagsJson.GetStringLength()));
                entity.setFlags(flags);
            }

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

        if(src.HasMember("lighting")) {
            lighting.ambient = Carrot::JSON::read<3, float>(src["lighting"]["ambient"]);
            lighting.raytracedShadows = src["lighting"]["raytracedShadows"].GetBool();
        }

        if(src.HasMember("skybox")) {
            std::string skyboxStr = src["skybox"].GetString();
            if(!Carrot::Skybox::safeFromName(skyboxStr, skybox)) {
                Carrot::Log::error("Unknown skybox: %s", skyboxStr.c_str());
            }
        }

        auto renderSystems = src["render_systems"].GetObject();
        for(const auto& [key, data] : renderSystems) {
            std::string systemName = key.GetString();
            auto system = systemLib.deserialise(systemName, data, world);
            world.addRenderSystem(std::move(system));
        }

        auto logicSystems = src["logic_systems"].GetObject();
        for(const auto& [key, data] : logicSystems) {
            std::string systemName = key.GetString();
            if(systemLib.has(systemName)) {
                auto system = systemLib.deserialise(systemName, data, world);
                world.addLogicSystem(std::move(system));
            } else {
                TODO; // add dummy system
            }
        }
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