//
// Created by jglrxavpok on 20/02/2021.
//

#include <engine/render/RenderContext.h>
#include "Component.h"

namespace Carrot::ECS {
    ComponentLibrary& getComponentLibrary() {
        static ComponentLibrary lib;
        return lib;
    }

    void ComponentLibrary::add(const Storage::ID& id, const Storage::DeserialiseFunction& deserialiseFunc, const Storage::CreateNewFunction& createNewFunc) {
        storage.add(id, deserialiseFunc, createNewFunc);
        // TODO: currently not registering components in a way that is visible to Lua scripts, do we want to?
    }

    void ComponentLibrary::remove(const Storage::ID& id) {
        storage.remove(id);
        // TODO: handle lua scripts?
    }

    std::unique_ptr<Component> ComponentLibrary::deserialise(const std::string& id, const rapidjson::Value& json, const Entity& entity) const {
        return storage.deserialise(id, json, entity);
    }

    std::unique_ptr<Component> ComponentLibrary::create(const Storage::ID& id, const Entity& entity) const {
        return storage.create(id, entity);
    }

    std::vector<std::string> ComponentLibrary::getAllIDs() const {
        return storage.getAllIDs();
    }

    void ComponentLibrary::registerBindings(sol::state& d, sol::usertype<Entity>& u) {
        for(const auto& f : usertypeDefinitionSuppliers) {
            f(d);
        }
        for(const auto& f : bindingFuncs) {
            f(d, u);
        }
    }
}