//
// Created by jglrxavpok on 08/01/2026.
//

#include <engine/io/actions/ActionDebug.h>

#include <imgui.h>
#include <engine/io/actions/ActionSet.h>

namespace Carrot::IO {
    void debugDrawActions() {
        if(ImGui::Begin("Debug IO Actions")) {
            const auto& sets = Carrot::IO::ActionSet::getSetList();
            for(const auto& set : sets) {
                std::string label = set->getName();
                if (set->isActive()) {
                    label += " (active)";
                }
                if(ImGui::TreeNode(label.c_str())) {
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
