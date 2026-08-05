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

    inline KinematicsComponent::KinematicsComponent(const Carrot::DocumentElement& doc, Entity entity): KinematicsComponent(std::move(entity)) {
            Carrot::DocumentElement::ObjectView objectView = doc.getAsObject();
            for (const BaseComponentPropertyReflection* pReflect : Reflection.getProperties()) {
                auto iter = objectView.find(pReflect->publicName);
                if (iter != objectView.end()) {
                    pReflect->deserialise(*this, iter->second);
                } else if (pReflect->mandatory) {
                    verify(false, Carrot::sprintf("Field %s is mandatory, but was not present in document", pReflect->name.c_str()));
                }
            }
        }

    inline Carrot::DocumentElement KinematicsComponent::serialise() const {
        Carrot::DocumentElement obj;

        for (const BaseComponentPropertyReflection* pReflect : Reflection.getProperties()) {
            obj[pReflect->publicName] = pReflect->serialise(*this);
        }
        return obj;
    }

    inline std::unique_ptr<Component> KinematicsComponent::duplicate(const Entity& newOwner) const {
        auto result = std::make_unique<KinematicsComponent>(newOwner);
        for (const BaseComponentPropertyReflection* pReflect : Reflection.getProperties()) {
            pReflect->duplicateProperty(*this, *result);
        }
        return result;
    }
}

ADD_COMPONENT_ID(Carrot::ECS, Kinematics)