//
// Created by jglrxavpok on 14/11/2021.
//
#include "LightComponent.h"
#include "engine/Engine.h"
#include "engine/utils/JSON.h"

namespace Carrot::ECS {
    LightComponent::LightComponent(Entity entity, std::shared_ptr<Render::LightHandle> light): IdentifiableComponent<LightComponent>(std::move(entity)), lightRef(light) {
        if(!lightRef) {
            lightRef = GetRenderer().getLighting().create();
            lightRef->light.enabled = true;
        }
    };

    LightComponent::LightComponent(const rapidjson::Value& json, Entity entity): IdentifiableComponent<LightComponent>(std::move(entity)) {
        lightRef = GetRenderer().getLighting().create();
        auto& light = lightRef->light;
        if(json.HasMember("enabled")) {
            light.enabled = json["enabled"].GetBool();
            light.color = Carrot::JSON::read<3, float>(json["color"]);
            light.intensity = json["intensity"].GetFloat();

            // TODO: handle invalid data
            light.type = Render::Light::fromString(json["type"].GetString());
        }
    }

    rapidjson::Value LightComponent::toJSON(rapidjson::Document& doc) const {
        rapidjson::Value obj{rapidjson::kObjectType};
        if(lightRef) { // components modified programmatically may not have this ref
            const auto& light = lightRef->light;
            obj.AddMember("enabled", static_cast<bool>(light.enabled), doc.GetAllocator());

            rapidjson::Value typeKey(Render::Light::nameOf(light.type), doc.GetAllocator());
            obj.AddMember("type", typeKey, doc.GetAllocator());

            obj.AddMember("color", Carrot::JSON::write(light.color, doc), doc.GetAllocator());
            obj.AddMember("intensity", light.intensity, doc.GetAllocator());
        }
        return obj;
    }

    void LightComponent::drawInspectorInternals(const Render::Context& renderContext, bool& modified) {
        if(lightRef) {
            auto& light = lightRef->light;

            bool enabled = light.enabled;
            if(ImGui::Checkbox("Enabled##inspector lightcomponent", &enabled)) {
                light.enabled = enabled;
            }

            ImGui::BeginDisabled(!enabled);


            ImGui::DragFloat("Intensity##inspector lightcomponent", &light.intensity);

            if(ImGui::BeginCombo("Light type##inspector lightcomponent", Render::Light::nameOf(light.type))) {
                auto selectable = [&](Render::LightType type) {
                    std::string id = Render::Light::nameOf(type);
                    id += "##inspector lightcomponent";
                    bool selected = type == light.type;
                    if(ImGui::Selectable(id.c_str(), &selected)) {
                        light.type = type;
                    }
                };

                selectable(Render::LightType::Point);
                selectable(Render::LightType::Directional);
                selectable(Render::LightType::Spot);

                ImGui::EndCombo();
            }

            float colorArr[3] = { light.color.r, light.color.g, light.color.b };
            glm::vec3& color = light.color;
            if(ImGui::ColorPicker3("Light color##inspector lightcomponent", colorArr)) {
                color = glm::vec3 { colorArr[0], colorArr[1], colorArr[2] };
                modified = true;
            }

            ImGui::EndDisabled();
        } else {
            ImGui::BeginDisabled();
            ImGui::TextWrapped("%s", "No light reference in this component. It must have been disabled explicitly.");
            ImGui::EndDisabled();
        }
    }
}