//
// Created by jglrxavpok on 06/10/2021.
//

#include "Scene.h"
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

    void Scene::serialise(rapidjson::Document& dest) const {
        dest.AddMember("world_data", world.getWorldData().toJSON(dest.GetAllocator()), dest.GetAllocator());

        rapidjson::Value entitiesMap(rapidjson::kObjectType);
        for(const auto& entity : world.getAllEntities()) {
            rapidjson::Value entityData(rapidjson::kObjectType);
            entityData.SetObject();
            auto components = world.getAllComponents(entity);

            for(const auto& comp : components) {
                rapidjson::Value key(comp->getName(), dest.GetAllocator());
                entityData.AddMember(key, comp->toJSON(dest), dest.GetAllocator());
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
            for(const auto& [componentNameKey, componentDataKey] : data) {
                std::string componentName = componentNameKey.GetString();
                if(componentName == "name" || componentName == "parent" || componentName == "flags") {
                    continue;
                }
                auto component = componentLib.deserialise(componentName, componentDataKey, entity);
                entity.addComponent(std::move(component));
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