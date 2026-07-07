//
// Created by jglrxavpok on 21/06/2026.
//

#include <engine/ecs/components/ui/UIBoxComponent.h>
#include <engine/ecs/components/ui/UICanvasComponent.h>
#include <panels/InspectorPanel.h>
#include <panels/inspector/EditorFunctions.h>

namespace Peeler {
    void editUICanvasComponent(EditContext& edition, const Carrot::Vector<Carrot::UI::UICanvasComponent*>& components) {
        multiEditField(edition, "In world", components,
            +[](Carrot::UI::UICanvasComponent& c) -> bool& { return c.inWorld; });

        // TODO: size
    }
    void editUIBoxComponent(EditContext& edition, const Carrot::Vector<Carrot::UI::UIBoxComponent*>& components) {
        multiEditField(edition, "Color", components,
            +[](Carrot::UI::UIBoxComponent& c) { return Helpers::RGBAColorWrapper{c.color};},
            +[](Carrot::UI::UIBoxComponent& c, const Helpers::RGBAColorWrapper& v) { c.color = v.rgba;});

        multiEditField(edition, "Child Gap", components,
            +[](Carrot::UI::UIBoxComponent& c) -> u16& { return c.childGap;});

        ImGui::SeparatorText("Padding");
        multiEditField(edition, "Left", components,
            +[](Carrot::UI::UIBoxComponent& c) -> u32& { return c.padding[0];});
        multiEditField(edition, "Right", components,
            +[](Carrot::UI::UIBoxComponent& c) -> u32& { return c.padding[1];});
        multiEditField(edition, "Bottom", components,
            +[](Carrot::UI::UIBoxComponent& c) -> u32& { return c.padding[2];});
        multiEditField(edition, "Top", components,
            +[](Carrot::UI::UIBoxComponent& c) -> u32& { return c.padding[3];});

        auto editSizing = [&](Carrot::UI::SizeType Carrot::UI::UIBoxComponent::* sizing, const char* id) {
            bool allSame = true;

            std::size_t sizingType = (components[0]->*sizing).index();
            for (Carrot::UI::UIBoxComponent* pComp : components) {
                if (sizingType != (pComp->*sizing).index()) {
                    allSame = false;
                    break;
                }
            }

            multiEditEnumField<Carrot::UI::UIBoxComponent, Carrot::UI::SizeTypeEnum>(edition, id, components,
                [sizing](Carrot::UI::UIBoxComponent& c) { return static_cast<Carrot::UI::SizeTypeEnum>((c.*sizing).index()); },
                [sizing](Carrot::UI::UIBoxComponent& c, const Carrot::UI::SizeTypeEnum& v) {
                    switch (v) {
                        case Carrot::UI::SizeTypeEnum::Fit:
                            c.*sizing = Carrot::UI::Fit{};
                            break;

                        case Carrot::UI::SizeTypeEnum::Fixed:
                            c.*sizing = Carrot::UI::Fixed{};
                            break;

                        case Carrot::UI::SizeTypeEnum::Grow:
                            c.*sizing = Carrot::UI::Grow{};
                            break;

                        case Carrot::UI::SizeTypeEnum::Percent:
                            c.*sizing = Carrot::UI::Percent{};
                            break;

                            default: TODO;
                    }
                }, [](const Carrot::UI::SizeTypeEnum& v) {
                    switch (v) {
                        case Carrot::UI::SizeTypeEnum::Fit:
                            return "Fit";

                        case Carrot::UI::SizeTypeEnum::Fixed:
                            return "Fixed";

                        case Carrot::UI::SizeTypeEnum::Grow:
                            return "Grow";

                        case Carrot::UI::SizeTypeEnum::Percent:
                            return "Percent";

                        default: TODO;
                    }
                }, {Carrot::UI::SizeTypeEnum::Fit, Carrot::UI::SizeTypeEnum::Fixed, Carrot::UI::SizeTypeEnum::Grow, Carrot::UI::SizeTypeEnum::Percent});

            // TODO: allow edition of params
            if (allSame) {

            } else {

            }
        };
        editSizing(&Carrot::UI::UIBoxComponent::width, "Width");
        editSizing(&Carrot::UI::UIBoxComponent::height, "Height");
        // TODO
    }
}
