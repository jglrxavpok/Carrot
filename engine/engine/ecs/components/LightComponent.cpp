//
// Created by jglrxavpok on 14/11/2021.
//
#include "LightComponent.h"
#include "engine/Engine.h"
#include "core/utils/JSON.h"
#include "core/utils/ImGuiUtils.hpp"

namespace Carrot::ECS {
    LightComponent::LightComponent(Entity entity, std::shared_ptr<Render::LightHandle> light): IdentifiableComponent<LightComponent>(std::move(entity)), lightRef(std::move(light)) {
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

            if(json.HasMember("point_light")) {
                auto params = json["point_light"].GetObject();
                light.constantAttenuation = params["constant_attenuation"].GetFloat();
                light.linearAttenuation = params["linear_attenuation"].GetFloat();
                light.quadraticAttenuation = params["quadratic_attenuation"].GetFloat();
            }
            if(json.HasMember("spot_light")) {
                auto params = json["spot_light"].GetObject();
                light.cutoffCosAngle = params["cutoff_cos_angle"].GetFloat();
                light.outerCutoffCosAngle = params["outer_cutoff_cos_angle"].GetFloat();
            }
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

            switch(light.type) {
                case Render::LightType::Point: {
                    rapidjson::Value params(rapidjson::kObjectType);
                    params.AddMember("constant_attenuation", light.constantAttenuation, doc.GetAllocator());
                    params.AddMember("linear_attenuation", light.linearAttenuation, doc.GetAllocator());
                    params.AddMember("quadratic_attenuation", light.quadraticAttenuation, doc.GetAllocator());

                    obj.AddMember("point_light", params, doc.GetAllocator());
                } break;

                case Render::LightType::Spot: {
                    rapidjson::Value params(rapidjson::kObjectType);
                    params.AddMember("cutoff_cos_angle", light.cutoffCosAngle, doc.GetAllocator());
                    params.AddMember("outer_cutoff_cos_angle", light.outerCutoffCosAngle, doc.GetAllocator());

                    obj.AddMember("spot_light", params, doc.GetAllocator());
                } break;
            }
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

            ImGui::BeginDisabled(!enabled);
            ImGui::DragFloat("Intensity##inspector lightcomponent", &light.intensity);

            if(ImGui::CollapsingHeader("Parameters")) {
                switch (light.type) {
                    case Render::LightType::Spot: {
                        float cutoffAngle = glm::acos(light.cutoffCosAngle);
                        float outerCutoffAngle = glm::acos(light.outerCutoffCosAngle);
                        if(ImGui::DragAngle("Cutoff angle", &cutoffAngle)) {
                            light.cutoffCosAngle = glm::cos(cutoffAngle);
                        }
                        if(ImGui::DragAngle("Outer cutoff angle", &outerCutoffAngle)) {
                            light.outerCutoffCosAngle = glm::cos(outerCutoffAngle);
                        }
                    } break;

                    case Render::LightType::Point: {
                        ImGui::DragFloat("Constant attenuation", &light.constantAttenuation);
                        ImGui::DragFloat("Linear attenuation", &light.linearAttenuation);
                        ImGui::DragFloat("Quadratic attenuation", &light.quadraticAttenuation);
                    } break;
                }
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

    std::shared_ptr<Render::LightHandle> LightComponent::duplicateLight(const Render::LightHandle& light) {
        auto clone = GetRenderer().getLighting().create();
        clone->light = light.light;
        return clone;
    }

    void LightComponent::reload() {
        if(savedLight) {
            lightRef = GetRenderer().getLighting().create();
            lightRef->light = savedLight.value();
        }
    }

    void LightComponent::unload() {
        savedLight = lightRef->light;
        lightRef = nullptr;
    }
}