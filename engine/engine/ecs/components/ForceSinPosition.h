//
// Created by jglrxavpok on 20/02/2021.
//

#pragma once

#include <engine/ecs/components/ComponentReflection.h>
#include <glm/glm.hpp>
#include <core/utils/JSON.h>

namespace Carrot::ECS {
    struct ForceSinPositionComponent: public ReflectionComponent<ForceSinPositionComponent> {
        using ReflectionComponent::ReflectionComponent;

        FIELD(glm::vec3, angularFrequency, "Angular Frequency", glm::vec3{1.0f});
        FIELD(glm::vec3, amplitude, "Amplitude", glm::vec3{1.0f});
        FIELD(glm::vec3, angularOffset, "Angular Offset", glm::vec3{0.0f});
        FIELD(glm::vec3, centerPosition, "Center Position", glm::vec3{0.0f});

        static void applyRewriteRules(Carrot::DocumentElement& doc) {
            doc.rename("angularFrequency", "Angular Frequency");
            doc.rename("amplitude", "Amplitude");
            doc.rename("angularOffset", "Angular Offset");
            doc.rename("centerPosition", "Center Position");
        }
    };
}

ADD_COMPONENT_ID(Carrot::ECS, ForceSinPosition);