//
// Created by jglrxavpok on 20/02/2022.
//

#include "CameraComponent.h"

#include <core/io/DocumentHelpers.h>
#include <core/utils/ImGuiUtils.hpp>

namespace Carrot::ECS {

    CameraComponent::CameraComponent(const Carrot::DocumentElement& doc, Entity entity): CameraComponent(std::move(entity)) {
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

    Carrot::DocumentElement CameraComponent::serialise() const {
        Carrot::DocumentElement obj;

        for (const BaseComponentPropertyReflection* pReflect : Reflection.getProperties()) {
            obj[pReflect->publicName] = pReflect->serialise(*this);
        }
        return obj;
    }

    std::unique_ptr<Component> CameraComponent::duplicate(const Entity& newOwner) const {
        auto result = std::make_unique<CameraComponent>(newOwner);
        for (const BaseComponentPropertyReflection* pReflect : Reflection.getProperties()) {
            pReflect->duplicateProperty(*this, *result);
        }
        return result;
    }

    glm::mat4 CameraComponent::makeProjectionMatrix(const Carrot::Render::Viewport& viewport) const {
        if(isOrthographic) {
            glm::mat4 projectionMatrix = glm::ortho(-orthoSize.x / 2.0f, orthoSize.x / 2.0f, -orthoSize.y / 2.0f, orthoSize.y / 2.0f, 0.0f, orthoSize.z);
            projectionMatrix[1][1] *= -1; // convert to Vulkan coordinates (from OpenGL)
            return projectionMatrix;
        } else {
            float aspectRatio = viewport.getSizef().x / viewport.getSizef().y;
            glm::mat4 projectionMatrix = glm::perspective(perspectiveFov, aspectRatio, perspectiveNear, perspectiveFar);
            projectionMatrix[1][1] *= -1; // convert to Vulkan coordinates (from OpenGL)
            return projectionMatrix;
        }
    }

    void CameraComponent::applyRewriteRules(Carrot::DocumentElement& doc) {
        // introduction of reflection system
        doc.rename("primary", "Primary");
        doc.rename("orthographic", "Orthographic");
        doc.rename("ortho_bounds", "Orthographic bounds");
        doc.rename("z_near", "Perspective Near");
        doc.rename("z_far", "Perspective Far");
        doc.rename("fov", "Perspective FOV");
        doc.rename("target_viewport", "Target Viewport");
    }
}