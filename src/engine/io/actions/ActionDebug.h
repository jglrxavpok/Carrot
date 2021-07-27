//
// Created by jglrxavpok on 27/07/2021.
//

#pragma once

#include <imgui.h>
#include <engine/io/actions/ActionSet.h>

namespace Carrot::IO {
    inline void debugDrawActions() {
        if(ImGui::Begin("Debug IO Actions")) {
            const auto& sets = Carrot::IO::ActionSet::getSetList();
            for(const auto& set : sets) {
                if(ImGui::TreeNode(set->getName().c_str())) {
                    for(const auto& boolInput : set->getBoolInputs()) {
                        bool checked = boolInput->isPressed();
                        ImGui::Checkbox(boolInput->getName().c_str(), &checked);
                    }

                    for(const auto& boolInput : set->getFloatInputs()) {
                        float value = boolInput->getValue();
                        ImGui::Text("%s = %f", boolInput->getName().c_str(), value);
                    }

                    for(const auto& boolInput : set->getVec2Inputs()) {
                        const auto& value = boolInput->getValue();
                        ImGui::Text("%s = { %f; %f }", boolInput->getName().c_str(), value.x, value.y);
                    }

                    ImGui::TreePop();
                }
            }
        }
        ImGui::End();
    }
}
