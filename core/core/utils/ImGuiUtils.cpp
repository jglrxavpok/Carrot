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

namespace ImGuiUtils {
    static ImColor CarrotColor{0xFF, 0x81, 0x46};

    const ImColor& getCarrotColor() {
        return CarrotColor;
    }

    static glm::vec4 DebugColorPalette[] = {
        glm::vec4(0,0,0,1),
        glm::vec4(189.0f / 255.0f, 236.0f / 255.0f, 182.0f / 255.0f, 1.0f),
        glm::vec4(108.0f / 255.0f, 112.0f / 255.0f,  89.0f / 255.0f, 1.0f),
        glm::vec4(203.0f / 255.0f, 208.0f / 255.0f, 204.0f / 255.0f, 1.0f),
        glm::vec4(250.0f / 255.0f, 210.0f / 255.0f,  01.0f / 255.0f, 1.0f),
        glm::vec4(220.0f / 255.0f, 156.0f / 255.0f,   0.0f / 255.0f, 1.0f),
        glm::vec4( 42.0f / 255.0f, 100.0f / 255.0f, 120.0f / 255.0f, 1.0f),
        glm::vec4(120.0f / 255.0f, 133.0f / 255.0f, 139.0f / 255.0f, 1.0f),
        glm::vec4(121.0f / 255.0f,  85.0f / 255.0f,  61.0f / 255.0f, 1.0f),
        glm::vec4(157.0f / 255.0f, 145.0f / 255.0f,  01.0f / 255.0f, 1.0f),
        glm::vec4(166.0f / 255.0f,  94.0f / 255.0f,  46.0f / 255.0f, 1.0f),
        glm::vec4(203.0f / 255.0f,  40.0f / 255.0f,  33.0f / 255.0f, 1.0f),
        glm::vec4(243.0f / 255.0f, 159.0f / 255.0f,  24.0f / 255.0f, 1.0f),
        glm::vec4(250.0f / 255.0f, 210.0f / 255.0f,  01.0f / 255.0f, 1.0f),
        glm::vec4(114.0f / 255.0f,  20.0f / 255.0f,  34.0f / 255.0f, 1.0f),
        glm::vec4( 64.0f / 255.0f,  58.0f / 255.0f,  58.0f / 255.0f, 1.0f),
        glm::vec4(157.0f / 255.0f, 161.0f / 255.0f, 170.0f / 255.0f, 1.0f),
        glm::vec4(164.0f / 255.0f, 125.0f / 255.0f, 144.0f / 255.0f, 1.0f),
        glm::vec4(248.0f / 255.0f,   0.0f / 255.0f,   0.0f / 255.0f, 1.0f),
        glm::vec4(120.0f / 255.0f,  31.0f / 255.0f,  25.0f / 255.0f, 1.0f),
        glm::vec4( 51.0f / 255.0f,  47.0f / 255.0f,  44.0f / 255.0f, 1.0f),
        glm::vec4(180.0f / 255.0f,  76.0f / 255.0f,  67.0f / 255.0f, 1.0f),
        glm::vec4(125.0f / 255.0f, 132.0f / 255.0f, 113.0f / 255.0f, 1.0f),
        glm::vec4(161.0f / 255.0f,  35.0f / 255.0f,  18.0f / 255.0f, 1.0f),
        glm::vec4(142.0f / 255.0f,  64.0f / 255.0f,  42.0f / 255.0f, 1.0f),
        glm::vec4(130.0f / 255.0f, 137.0f / 255.0f, 143.0f / 255.0f, 1.0f),
    };

    glm::vec4 getDebugColorFromPalette(uint64_t index) {
        if(index == 0) {
            return DebugColorPalette[0];
        }
        return DebugColorPalette[uint8_t(index % 25ul + 1)];
    }

    ImColor getDebugColorFromPaletteAsImColor(uint64_t index) {
        glm::vec4 c = getDebugColorFromPalette(index);
        return {c.r, c.g, c.b, c.a};
    }
}