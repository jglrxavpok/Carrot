//
// Created by jglrxavpok on 21/06/2026.
//

#include "CameraTypeLayer.h"

#include <Peeler.h>
#include <core/Macros.h>
#include <core/utils/ImGuiUtils.hpp>

namespace Peeler {

    void CameraTypeLayer::draw(const Carrot::Render::Context& renderContext, float startX, float startY) {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(5.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0.0, 0.0));
        CLEANUP(ImGui::PopStyleColor());
        CLEANUP(ImGui::PopStyleVar());

        const float dropdownSize = 200.0f;
        ImVec2 selectionWindowPos;
        selectionWindowPos.x = startX + ImGui::GetContentRegionAvail().x - dropdownSize - 10.0f /*some padding*/;
        selectionWindowPos.y = startY + 10.0f;

        ImGui::SetNextWindowPos(selectionWindowPos, ImGuiCond_Always);
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking
                                       | ImGuiWindowFlags_NoTitleBar
                                       | ImGuiWindowFlags_NoCollapse
                                       | ImGuiWindowFlags_NoScrollbar
                                       | ImGuiWindowFlags_AlwaysAutoResize;

        ImGui::Begin("#camera types", nullptr, windowFlags);
        CLEANUP(ImGui::End());

        const char* previewText = nullptr;
        if (editor.currentCameraType == Application::CameraType::FreeCam) {
            previewText = "Free camera";
        } else if (editor.currentCameraType == Application::CameraType::UI) {
            previewText = "2D";
        } else {
            TODO;
        }

        ImGui::SetNextItemWidth(dropdownSize);
        if (ImGui::BeginCombo("##Camera type", previewText)) {
            if (ImGui::Selectable("Free camera", editor.currentCameraType == Application::CameraType::FreeCam)) {
                editor.currentCameraType = Application::CameraType::FreeCam;
            }
            if (ImGui::Selectable("2D", editor.currentCameraType == Application::CameraType::UI)) {
                editor.currentCameraType = Application::CameraType::UI;
            }
            ImGui::EndCombo();
        }
    }

} // Peeler