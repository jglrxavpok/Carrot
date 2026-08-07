//
// Created by jglrxavpok on 21/06/2026.
//

#pragma once
#include <engine/ecs/components/ComponentReflection.h>
#include <engine/render/RenderPassData.h>

namespace Carrot::UI {
    struct UICanvasComponent: public ECS::ReflectionComponent<UICanvasComponent> {
        Render::TextureSize size;
        bool inWorld = false;

        using ReflectionComponent::ReflectionComponent;
        UICanvasComponent(const Carrot::DocumentElement& json, Carrot::ECS::Entity entity);
        Carrot::DocumentElement serialise() const override;
        std::unique_ptr<Component> duplicate(const Carrot::ECS::Entity& newOwner) const override;
    };
}

ADD_COMPONENT_ID(Carrot::UI, UICanvas)
