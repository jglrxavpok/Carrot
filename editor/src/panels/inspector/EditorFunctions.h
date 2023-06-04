//
// Created by jglrxavpok on 31/05/2023.
//

#pragma once

#include <panels/InspectorPanel.h>
#include <core/utils/ImGuiUtils.hpp>

namespace Peeler {
    /**
     * Register all edition functions below to the given inspector
     */
    void registerEditionFunctions(InspectorPanel& inspector);
}