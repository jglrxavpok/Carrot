//
// Created by jglrxavpok on 06/08/2021.
//

#pragma once

#include "Component.h"
#include <glm/glm.hpp>
#include <core/io/DocumentHelpers.h>

namespace Carrot::ECS {
    BEGIN_COMPONENT(Kinematics)
        FIELD(glm::vec3, velocity, "Velocity", {});

        static void applyRewriteRules(Carrot::DocumentElement& doc) {
            doc.rename("velocity", "Velocity");
        }
    END_COMPONENT
}

ADD_COMPONENT_ID(Carrot::ECS, Kinematics)