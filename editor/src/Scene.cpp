//
// Created by jglrxavpok on 06/10/2021.
//

#include "Scene.h"

namespace Peeler {
    void Scene::tick(double frameTime) {
        world.tick(frameTime);
    }

    void Scene::onFrame(const Carrot::Render::Context& renderContext) {
        world.onFrame(renderContext);
    }

    Scene& Scene::operator=(const Scene& toCopy) {
        world = toCopy.world;
        return *this;
    }

    void Scene::clear() {
        *this = Scene();
    }

    void Scene::serialise(rapidjson::Document& dest) const {
        dest.SetObject();

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

            rapidjson::Value key(entity.getID().toString(), dest.GetAllocator());
            entitiesMap.AddMember(key, entityData, dest.GetAllocator());
        }

        dest.AddMember("entities", entitiesMap, dest.GetAllocator());
    }
}