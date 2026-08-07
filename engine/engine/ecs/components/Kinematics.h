//
// Created by jglrxavpok on 06/08/2021.
//

#pragma once

#include <core/io/DocumentHelpers.h>
#include <glm/glm.hpp>
#include <engine/ecs/components/ComponentReflection.h>

namespace Carrot::ECS {
    struct KinematicsComponent: public ReflectionComponent<KinematicsComponent> {
        using ReflectionComponent::ReflectionComponent;

        FIELD(glm::vec3, velocity, "Velocity", {});

        static void applyRewriteRules(Carrot::DocumentElement& doc) {
            doc.rename("velocity", "Velocity");
        }
    };
}

ADD_COMPONENT_ID(Carrot::ECS, Kinematics)