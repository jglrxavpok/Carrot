//
// Created by jglrxavpok on 31/05/2023.
//

#pragma once

#include <panels/InspectorPanel.h>
#include <core/utils/ImGuiUtils.hpp>

namespace Carrot::ECS {
    class CSharpComponent;
}

namespace Peeler {
    /**
     * Register all edition functions below to the given inspector
     */
    void registerEditionFunctions(InspectorPanel& inspector);

    void editCSharpComponent(EditContext& edition, Carrot::ECS::CSharpComponent* component);
}