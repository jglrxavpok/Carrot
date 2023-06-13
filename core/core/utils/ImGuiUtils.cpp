//
// Created by jglrxavpok on 06/06/2023.
//

#include "ImGuiUtils.hpp"
#include <unordered_map>

namespace ImGui {

    bool ImageToggleButton(const char* label, ImTextureID textureID, bool* state, const ImVec2& imageSize, ImVec2 buttonSize) {
        const ImColor textColor = ImColor(255, 255, 255, 255); // TODO: configurable
        const char* textEnd = ImGui::FindRenderedTextEnd(label);

        const bool hasText = textEnd != label;

        const ImGuiStyle& style = ImGui::GetStyle();
        const float spacing = hasText ? style.ItemInnerSpacing.x : 0.0f;
        const ImVec2& padding = style.FramePadding;
        if(buttonSize.x == 0.0f && buttonSize.y == 0.0f) {
            const ImVec2 textSize = ImGui::CalcTextSize(label, textEnd);
            buttonSize.x = textSize.x + imageSize.x + spacing + padding.x * 2;
            buttonSize.y = std::max(textSize.y, imageSize.y) + padding.y * 2;
        }

        ImVec2 cursorPos = ImGui::GetCursorScreenPos();
        bool clicked = ImGui::InvisibleButton(label, buttonSize, ImGuiButtonFlags_None);

        if(clicked) {
            *state = ! *state;
        }

        ImDrawList* drawList = ImGui::GetWindowDrawList();

        ImGuiCol_ colorState = ImGuiCol_Button;

        if(*state) {
            colorState = ImGuiCol_ButtonActive;
        } else if(ImGui::IsItemHovered()) {
            colorState = ImGuiCol_ButtonHovered;
        } else {
            colorState = ImGuiCol_Button;
        }

        drawList->AddRectFilled(cursorPos, cursorPos + buttonSize, ImGui::GetColorU32(style.Colors[colorState]));
        drawList->AddImage(textureID, cursorPos + padding, cursorPos + padding + imageSize);
        drawList->AddText(cursorPos + padding + ImVec2(spacing + imageSize.x, 0), textColor, label, textEnd); // TODO: center text

        return clicked;
    }
}