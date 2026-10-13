//
// Created by jglrxavpok on 23/09/2026.
//

#pragma once
#include <engine/ecs/components/ComponentReflection.h>

namespace Carrot::ECS {
    struct PortalComponent: public ReflectionComponent<PortalComponent> {
        using ReflectionComponent::ReflectionComponent;

        FIELD(Carrot::ECS::Entity, otherPortal, "OtherPortal", {});

        constexpr inline static i32 MaxRecursion = 2; // TODO: bump higher once memory issues are fixed

        struct CameraData {
            glm::mat4 finalCameraTransform{1.0f};
            Math::Plane worldSpaceClipPlane{};
        };
        std::array<CameraData, MaxRecursion> cameraData;
    };
}

ADD_COMPONENT_ID(Carrot::ECS, Portal);
