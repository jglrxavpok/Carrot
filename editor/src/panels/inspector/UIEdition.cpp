//
// Created by jglrxavpok on 21/06/2026.
//

#include <engine/ecs/components/ui/UICanvasComponent.h>
#include <panels/InspectorPanel.h>
#include <panels/inspector/EditorFunctions.h>

namespace Peeler {
    void editUICanvasComponent(EditContext& edition, const Carrot::Vector<Carrot::UI::UICanvasComponent*>& components) {
        multiEditField(edition, "In world", components,
            +[](Carrot::UI::UICanvasComponent& c) -> bool& { return c.inWorld; });

        // TODO: size
    }
}
