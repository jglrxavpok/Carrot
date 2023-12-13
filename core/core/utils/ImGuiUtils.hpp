//
// Created by jglrxavpok on 08/05/2021.
//

#pragma once

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <core/utils/stringmanip.h>
#include <core/math/Constants.h>

inline ImVec2 operator+(const ImVec2& lhs, const ImVec2& rhs) {
    return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y);
}

inline ImVec2 operator-(const ImVec2& lhs, const ImVec2& rhs) {
    return ImVec2(lhs.x - rhs.x, lhs.y - rhs.y);
}

namespace ImGui {
    inline int StdStringInputCallback( ImGuiInputTextCallbackData* data ) {
        switch( data->EventFlag ) {
            case ImGuiInputTextFlags_CallbackResize:
                // from imgui_demo.cpp
                if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
                    auto* my_str = (ImVector<char>*)data->UserData;
                    IM_ASSERT(my_str->begin() == data->Buf);
                    my_str->resize(data->BufSize); // NB: On resizing calls, generally data->BufSize == data->BufTextLen + 1
                    data->Buf = my_str->begin();
                }
                break;
        }

        return 0;
    }

    inline bool InputText(const char* label, std::string& str, ImGuiInputTextFlags flags = ImGuiInputTextFlags_None) {
        return ::ImGui::InputText(label, &str, flags, StdStringInputCallback);
    }

    inline bool InputTextMultiline(const char* label, std::string& str, ImGuiInputTextFlags flags = ImGuiInputTextFlags_None) {
        return ::ImGui::InputTextMultiline(label, &str, ImVec2(0,0), flags, StdStringInputCallback);
    }

    inline void UnformattedWTextWrapped(std::u8string_view wideText) {
        std::string converted = Carrot::toString(wideText);
        ::ImGui::TextWrapped("%s", converted.c_str());
    }

    inline void UnformattedWText(const std::u8string& wideText) {
        std::string converted = Carrot::toString(wideText);
        ::ImGui::Text("%s", converted.c_str());
    }

    inline bool DragAngle(const char* label, float* v_rad, float v_speed = 1.0f, float v_degrees_min = 0.0f, float v_degrees_max = 360.0f, const char* format = "%.0f deg", ImGuiSliderFlags flags = 0) {
        if(format == nullptr) {
            format = "%.0f deg";
        }
        float v_deg = *v_rad * Carrot::Math::Rad2Degrees;
        if(ImGui::DragFloat(label, &v_deg, v_speed, v_degrees_min, v_degrees_max, format, flags)) {
            *v_rad = v_deg * Carrot::Math::Degrees2Rad;
            return true;
        }
        return false;
    }

    /**
     * Toggle-able button with an icon
     * @param label text to display inside button. Also used by ImGui as an ID (so use ##something to differentiate between buttons if needed)
     * @param textureID texture to use as an icon
     * @param state boolean to set/unset depending on whether the button is pressed
     * @param imageSize size of the icon
     * @param buttonSize total size of the button. Set to 0,0 to use imageSize + width of text
     * @return true iif button was clicked
     */
    bool ImageToggleButton(const char* label, ImTextureID textureID, bool* state, const ImVec2& imageSize, ImVec2 buttonSize = ImVec2(0, 0));

    // from https://github.com/ocornut/imgui/issues/2644
    inline bool CheckBoxTristate(const char* label, int* v_tristate)
    {
        bool ret;
        if (*v_tristate == -1)
        {
            ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, true);
            bool b = false;
            ret = ImGui::Checkbox(label, &b);
            if (ret)
                *v_tristate = 1;
            ImGui::PopItemFlag();
        }
        else
        {
            bool b = (*v_tristate != 0);
            ret = ImGui::Checkbox(label, &b);
            if (ret)
                *v_tristate = (int)b;
        }
        return ret;
    }
}