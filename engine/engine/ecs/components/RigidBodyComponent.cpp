//
// Created by jglrxavpok on 31/12/2021.
//

#include "RigidBodyComponent.h"
#include <imgui.h>
#include <map>

namespace Carrot::ECS {

    RigidBodyComponent::RigidBodyComponent(const rapidjson::Value& json, Entity entity): RigidBodyComponent(std::move(entity)) {
        TODO
    }

    rapidjson::Value RigidBodyComponent::toJSON(rapidjson::Document& doc) const {
        TODO
    }

    void RigidBodyComponent::drawInspectorInternals(const Render::Context &renderContext, bool &modified) {
        const reactphysics3d::BodyType type = rigidbody.getBodyType();

        if(ImGui::BeginCombo("Type##rigidbodycomponent", getTypeName(type))) {
            for(reactphysics3d::BodyType bodyType : { reactphysics3d::BodyType::DYNAMIC, reactphysics3d::BodyType::KINEMATIC, reactphysics3d::BodyType::STATIC }) {
                bool selected = bodyType == type;
                if(ImGui::Selectable(getTypeName(bodyType), selected)) {
                    rigidbody.setBodyType(bodyType);
                    modified = true;
                }
            }
            ImGui::EndCombo();
        }
    }

    const char* RigidBodyComponent::getTypeName(reactphysics3d::BodyType type) {
        switch (type) {
            case reactphysics3d::BodyType::DYNAMIC:
                return "Dynamic";

            case reactphysics3d::BodyType::KINEMATIC:
                return "Kinematic";

            case reactphysics3d::BodyType::STATIC:
                return "Static";
        }
        verify(false, "missing case");
        return "Dynamic";
    }

    reactphysics3d::BodyType RigidBodyComponent::getTypeFromName(const std::string& name) {
        if(_stricmp(name.c_str(), "Dynamic") == 0) {
            return reactphysics3d::BodyType::DYNAMIC;
        }
        if(_stricmp(name.c_str(), "Kinematic") == 0) {
            return reactphysics3d::BodyType::KINEMATIC;
        }
        if(_stricmp(name.c_str(), "Static") == 0) {
            return reactphysics3d::BodyType::STATIC;
        }
        verify(false, "invalid string");
        return reactphysics3d::BodyType::DYNAMIC;
    }

}