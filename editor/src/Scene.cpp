//
// Created by jglrxavpok on 06/10/2021.
//

#include "Scene.h"
#include "core/utils/JSON.h"

namespace Peeler {
    void Scene::tick(double frameTime) {
        world.tick(frameTime);
    }

    void Scene::onFrame(const Carrot::Render::Context& renderContext) {
        world.onFrame(renderContext);
    }

    void Scene::clear() {
        *this = Scene();
    }

    rapidjson::Value Scene::serialise(rapidjson::Document& doc) const {
        rapidjson::Value dest(rapidjson::kObjectType);

        rapidjson::Value entitiesMap(rapidjson::kObjectType);
        for(const auto& entity : world.getAllEntities()) {
            rapidjson::Value entityData(rapidjson::kObjectType);
            entityData.SetObject();
            auto components = world.getAllComponents(entity);

            for(const auto& comp : components) {
                rapidjson::Value key(comp->getName(), doc.GetAllocator());
                entityData.AddMember(key, comp->toJSON(doc), doc.GetAllocator());
            }

            if(auto parent = entity.getParent()) {
                rapidjson::Value parentID(parent->getID().toString(), doc.GetAllocator());
                entityData.AddMember("parent", parentID, doc.GetAllocator());
            }

            rapidjson::Value nameKey(std::string(entity.getName()), doc.GetAllocator());
            entityData.AddMember("name", nameKey, doc.GetAllocator());

            rapidjson::Value key(entity.getID().toString(), doc.GetAllocator());
            entitiesMap.AddMember(key, entityData, doc.GetAllocator());
        }

        dest.AddMember("entities", entitiesMap, doc.GetAllocator());

        rapidjson::Value lightingObj(rapidjson::kObjectType);
        {
            lightingObj.AddMember("ambient", Carrot::JSON::write(lighting.ambient, doc), doc.GetAllocator());
            lightingObj.AddMember("raytracedShadows", lighting.raytracedShadows, doc.GetAllocator());
        }

        dest.AddMember("lighting", lightingObj, doc.GetAllocator());

        rapidjson::Value logicSystems{ rapidjson::kObjectType };
        for(auto& system : world.getLogicSystems()) {
            if(system->shouldBeSerialized()) {
                rapidjson::Value key(system->getName(), doc.GetAllocator());
                logicSystems.AddMember(key, system->toJSON(doc.GetAllocator()), doc.GetAllocator());
            }
        }
        dest.AddMember("logic_systems", logicSystems, doc.GetAllocator());

        rapidjson::Value renderSystems{ rapidjson::kObjectType };
        for(auto& system : world.getRenderSystems()) {
            if(system->shouldBeSerialized()) {
                rapidjson::Value key(system->getName(), doc.GetAllocator());
                renderSystems.AddMember(key, system->toJSON(doc.GetAllocator()), doc.GetAllocator());
            }
        }
        dest.AddMember("render_systems", renderSystems, doc.GetAllocator());
        return dest;
    }

    void Scene::deserialise(const rapidjson::Value& src) {
        assert(src.IsObject());
        clear();
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
            for(const auto& [componentNameKey, componentDataKey] : data) {
                std::string componentName = componentNameKey.GetString();
                if(componentName == "name" || componentName == "parent") {
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

        auto renderSystems = src["render_systems"].GetObject();
        for(const auto& [key, data] : renderSystems) {
            std::string systemName = key.GetString();
            auto system = systemLib.deserialise(systemName, data, world);
            world.addRenderSystem(std::move(system));
        }

        auto logicSystems = src["logic_systems"].GetObject();
        for(const auto& [key, data] : logicSystems) {
            std::string systemName = key.GetString();
            auto system = systemLib.deserialise(systemName, data, world);
            world.addLogicSystem(std::move(system));
        }
    }

    void Scene::load() {
        world.reloadSystems();
    }

    void Scene::unload() {
        world.unloadSystems();
    }
}