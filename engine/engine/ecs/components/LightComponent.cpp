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