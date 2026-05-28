//
// Created by jglrxavpok on 06/03/2021.
//

#pragma once

#include "Component.h"
#include "engine/render/lighting/Lights.h"

namespace Carrot::ECS {
    struct LightComponent: public IdentifiableComponent<LightComponent> {
        std::shared_ptr<Render::LightHandle> lightRef;

        explicit LightComponent(Entity entity, std::shared_ptr<Render::LightHandle> light = nullptr);

        explicit LightComponent(const Carrot::DocumentElement& doc, Entity entity);

        Carrot::DocumentElement serialise() const override;

        const char *const getName() const override {
            return "LightComponent";
        }

        std::unique_ptr<Component> duplicate(const Entity& newOwner) const override {
            auto result = std::make_unique<LightComponent>(newOwner, duplicateLight(newOwner, *lightRef));
            return result;
        }

    private:
        std::shared_ptr<Render::LightHandle> duplicateLight(const Entity& newOwner, const Render::LightHandle& light) const;
    };
}

template<>
inline const char* Carrot::Identifiable<Carrot::ECS::LightComponent>::getStringRepresentation() {
    return "LightComponent";
}