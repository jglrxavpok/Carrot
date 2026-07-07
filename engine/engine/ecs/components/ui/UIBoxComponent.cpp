//
// Created by jglrxavpok on 22/06/2026.
//

#include "UIBoxComponent.h"

#include <core/io/DocumentHelpers.h>

namespace Carrot::UI {
    UIBoxComponent::UIBoxComponent(const Carrot::DocumentElement& json, Carrot::ECS::Entity entity): UIBoxComponent(std::move(entity)) {
        padding = Carrot::DocumentHelpers::read<4, u32>(json["padding"]);
        color = Carrot::DocumentHelpers::read<4, float>(json["color"]);
        childGap = json["child_gap"].getAsInt64();

        auto readRange = [&](const Carrot::DocumentElement& element) -> Range {
            return Range { static_cast<float>(element["min"].getAsDouble()), static_cast<float>(element["max"].getAsDouble())};
        };

        auto readSize = [&](const Carrot::DocumentElement& element) -> SizeType {
            std::string_view typeStr = element["type"].getAsString();
            if (typeStr == "fit") {
                return Fit{ readRange(element["range"])};
            } else if (typeStr == "grow") {
                return Grow{readRange(element["range"])};
            } else if (typeStr == "percent") {
                return Percent{static_cast<float>(element["ratio"].getAsDouble())};
            } else if (typeStr == "fixed") {
                return Fixed{};
            } else {
                TODO;
            }
        };
        width = readSize(json["width"]);
        height = readSize(json["height"]);
    }

    Carrot::DocumentElement UIBoxComponent::serialise() const {
        Carrot::DocumentElement result;
        result["padding"] = Carrot::DocumentHelpers::write<4, u32>(padding);
        result["color"] = Carrot::DocumentHelpers::write<4, float>(color);
        result["child_gap"] = childGap;

        auto writeRange = [&](const Range& range) {
            Carrot::DocumentElement element;
            element["min"] = range.min;
            element["max"] = range.max;
            return element;
        };

        auto writeSize = [&](const SizeType& size) {
            Carrot::DocumentElement element;
            if (const Fit* pFit = std::get_if<Fit>(&size)) {
                element["type"] = "fit";
                element["range"] = writeRange(pFit->range);
            } else if (const Grow* pGrow = std::get_if<Grow>(&size)) {
                element["type"] = "grow";
                element["range"] = writeRange(pGrow->range);
            } else if (const Percent* pPercent = std::get_if<Percent>(&size)) {
                element["type"] = "percent";
                element["ratio"] = pPercent->ratio;
            } else if (const Fixed* pFixed = std::get_if<Fixed>(&size)) {
                element["type"] = "fixed";
            } else {
                TODO;
            }

            return element;
        };

        result["width"] = writeSize(width);
        result["height"] = writeSize(height);
        return result;
    }

    std::unique_ptr<ECS::Component> UIBoxComponent::duplicate(const Carrot::ECS::Entity& newOwner) const {
        std::unique_ptr<UIBoxComponent> clone = std::make_unique<UIBoxComponent>(newOwner);
        clone->width = width;
        clone->height = height;
        clone->color = color;
        clone->padding = padding;
        clone->childGap = childGap;
        return clone;
    }


} // Carrot::UI