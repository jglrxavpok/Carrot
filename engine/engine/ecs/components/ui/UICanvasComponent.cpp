//
// Created by jglrxavpok on 21/06/2026.
//

#include "UICanvasComponent.h"

namespace Carrot::UI {
    /*
    UICanvasComponent::UICanvasComponent(const Carrot::DocumentElement& json, Carrot::ECS::Entity entity): UICanvasComponent(entity) {
        inWorld = json["in_world"].getAsBool();

        bool isProportionalToViewport = json["proportional_to_viewport"].getAsBool();
        float w = json["width"].getAsDouble();
        float h = json["height"].getAsDouble();
        size.type = isProportionalToViewport ? Render::TextureSize::Type::ViewportProportional : Render::TextureSize::Type::Fixed;
        size.width = w;
        size.height = h;
        size.depth = 1;
    }

    Carrot::DocumentElement UICanvasComponent::serialise() const {
        Carrot::DocumentElement result;
        result["in_world"] = inWorld;
        result["proportional_to_viewport"] = size.type == Render::TextureSize::Type::ViewportProportional;
        result["width"] = size.width;
        result["height"] = size.height;
        return result;
    }

    std::unique_ptr<ECS::Component> UICanvasComponent::duplicate(const Carrot::ECS::Entity& newOwner) const {
        std::unique_ptr<UICanvasComponent> clone = std::make_unique<UICanvasComponent>(newOwner);
        clone->size = size;
        clone->inWorld = inWorld;
        return clone;
    }
*/
}

