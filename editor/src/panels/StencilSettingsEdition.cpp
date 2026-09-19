//
// Created by jglrxavpok on 19/09/2026.
//

#include "StencilSettingsEdition.h"
#include <core/utils/ImGuiUtils.hpp>
#include <engine/render/StencilSettings.h>

namespace Peeler {
    bool editStencilSettings(Carrot::Render::StencilSettings& settings, bool& enabled) {
        bool stencilModified = false;
        if (ImGui::Checkbox("Enable", &enabled)) {
            stencilModified = true;
        }

        ImGui::BeginDisabled(!enabled);

        if (ImGui::Checkbox("Write", &settings.stencilWrite)) {
            stencilModified = true;
        }
        if (ImGui::Checkbox("Compare", &settings.stencilCompare)) {
            stencilModified = true;
        }
        if (ImGui::InputScalar("Reference", ImGuiDataType_U8, &settings.stencilValue)) {
            stencilModified = true;
        }

        Carrot::Render::StencilOperation& viewportOp = settings.stencilOperation;
        if (ImGui::BeginCombo("Operation", Carrot::Render::toString(viewportOp))) {
            auto handleOp = [&](const Carrot::Render::StencilOperation& op) {
                if (ImGui::Selectable(toString(op), op == viewportOp)) {
                    viewportOp = op;
                    stencilModified = true;
                }
            };
            handleOp(Carrot::Render::StencilOperation::AlwaysPass);
            handleOp(Carrot::Render::StencilOperation::Equal);
            ImGui::EndCombo();
        }

        ImGui::EndDisabled();
        return stencilModified;
    }
}
