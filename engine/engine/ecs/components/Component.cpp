//
// Created by jglrxavpok on 20/02/2021.
//

#include <engine/render/RenderContext.h>
#include "Component.h"
#include <imgui/imgui.h>

namespace Carrot::ECS {
    void Component::drawInspector(const Carrot::Render::Context& renderContext, bool& shouldKeep, bool& modified) {
        shouldKeep = true;
        std::string s = getName();
        s += "##" + getEntity().getID().toString();
        if(ImGui::CollapsingHeader(s.c_str(), &shouldKeep)) {
            drawInspectorInternals(renderContext, modified);
        }

        if(!shouldKeep) {
            shouldKeep = false;
        }
    }

    ComponentLibrary& getComponentLibrary() {
        static ComponentLibrary lib;
        return lib;
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