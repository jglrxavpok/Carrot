//
// Created by jglrxavpok on 14/11/2021.
//
#include "LightComponent.h"

#include <core/io/DocumentHelpers.h>

#include "engine/Engine.h"
#include "core/utils/JSON.h"
#include "core/utils/ImGuiUtils.hpp"

namespace Carrot::ECS {
    LightComponent::LightComponent(Entity entity, Render::LightHandle light): IdentifiableComponent<LightComponent>(std::move(entity)), lightRef(std::move(light)) {
        if(!lightRef) {
            lightRef = entity.getWorld().getLighting().create();
            lightRef->flags = Render::LightFlags::Enabled;
        }
    };

    LightComponent::LightComponent(const Carrot::DocumentElement& doc, Entity entity): IdentifiableComponent<LightComponent>(std::move(entity)) {
        lightRef = entity.getWorld().getLighting().create();
        auto& light = *lightRef;
        if(doc.contains("enabled")) {
            light.flags = doc["enabled"].getAsBool() ? Render::LightFlags::Enabled : Render::LightFlags::None;
            light.color = Carrot::DocumentHelpers::read<3, float>(doc["color"]);
            light.intensity = doc["intensity"].getAsDouble();

            // TODO: handle invalid data
            light.type = Render::Light::fromString(doc["type"].getAsString());

            if(doc.contains("point_light")) {
                auto& params = doc["point_light"];
                light.point.constantAttenuation = params["constant_attenuation"].getAsDouble();
                light.point.linearAttenuation = params["linear_attenuation"].getAsDouble();
                light.point.quadraticAttenuation = params["quadratic_attenuation"].getAsDouble();
            }
            if(doc.contains("spot_light")) {
                auto& params = doc["spot_light"];
                light.spot.cutoffCosAngle = params["cutoff_cos_angle"].getAsDouble();
                light.spot.outerCutoffCosAngle = params["outer_cutoff_cos_angle"].getAsDouble();
            }
        }
    }

    Carrot::DocumentElement LightComponent::serialise() const {
        Carrot::DocumentElement obj;
        if(lightRef) { // components modified programmatically may not have this ref
            const auto& light = *lightRef;
            obj["enabled"] = static_cast<bool>((light.flags & Render::LightFlags::Enabled) != Render::LightFlags::None);

            obj["type"] = Render::Light::nameOf(light.type);

            obj["color"] = Carrot::DocumentHelpers::write(light.color);
            obj["intensity"] = light.intensity;

            switch(light.type) {
                case Render::LightType::Point: {
                    Carrot::DocumentElement params;
                    params["constant_attenuation"] = light.point.constantAttenuation;
                    params["linear_attenuation"] = light.point.linearAttenuation;
                    params["quadratic_attenuation"] = light.point.quadraticAttenuation;

                    obj["point_light"] = params;
                } break;

                case Render::LightType::Spot: {
                    Carrot::DocumentElement params;
                    params["cutoff_cos_angle"] = light.spot.cutoffCosAngle;
                    params["outer_cutoff_cos_angle"] = light.spot.outerCutoffCosAngle;

                    obj["spot_light"] = params;
                } break;
            }
        }
        return obj;
    }

    Render::LightHandle LightComponent::duplicateLight(const Entity& newOwner, const Render::LightHandle& light) const {
        Entity newOwnerModifiable = newOwner;
        auto clone = newOwnerModifiable.getWorld().getLighting().create();
        // TODO: a bit ugly, need a solution for HandleBased objects that are copyable
        static_cast<Render::GPULight&>(*clone) = static_cast<Render::GPULight&>(*light);
        return clone;
    }
}