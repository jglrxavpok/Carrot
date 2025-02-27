//
// Created by jglrxavpok on 12/02/2025.
//

#include "Widgets.h"

#include <IconsFontAwesome5.h>
#include <imgui.h>
#include <core/Macros.h>
#include <engine/utils/Macros.h>
#include <engine/render/VulkanRenderer.h>

namespace Carrot::Widgets {
    std::optional<MessageBoxButtons> drawMessageBox(const char* title, const char* caption, MessageBoxIcon type, MessageBoxButtons buttons) {
        static std::unordered_map<MessageBoxButtons, std::string> texts = {
            {MessageBoxButtons::Ok, "Ok"},
            {MessageBoxButtons::Yes, "Yes"},
            {MessageBoxButtons::Save, ICON_FA_SAVE "  Save"},
            {MessageBoxButtons::DontSave, ICON_FA_TIMES "  Don't save"},
            {MessageBoxButtons::No, "No"},
            {MessageBoxButtons::Cancel, ICON_FA_BAN "  Cancel"},
        };
        std::optional<MessageBoxButtons> result;
        if (!ImGui::IsPopupOpen(title))
            ImGui::OpenPopup(title);
        ImGui::SetNextWindowSize(ImVec2(500, 120), ImGuiCond_Always/*todo: appearing/once*/);
        if (ImGui::BeginPopupModal(title)) {
            {
                ImGui::PushFont(GetRenderer().getImGuiBackend().getBigIconsFont());
                switch (type)
                {
                    case MessageBoxIcon::Warning:
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
                        ImGui::Text(ICON_FA_EXCLAMATION_TRIANGLE);
                    break;

                    case MessageBoxIcon::Error:
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
                        ImGui::Text(ICON_FA_EXCLAMATION_CIRCLE);
                    break;

                    case MessageBoxIcon::Info:
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 1.0f, 1.0f));
                        ImGui::Text(ICON_FA_INFO_CIRCLE);
                    break;
                }
                ImGui::PopStyleColor();
                ImGui::PopFont();

                ImGui::SetWindowFontScale(1);
            }
            ImGui::SameLine();
            ImGui::TextWrapped("%s", caption);

            ImGui::Separator();

            bool isFirst = true;
#define BUTTON(t)                                                                                                       \
            if(+(buttons & t)) {                                                                                        \
                if(!isFirst) {                                                                                          \
                    ImGui::SameLine();                                                                                  \
                }                                                                                                       \
                isFirst = false;                                                                                        \
                if(ImGui::Button(texts[t].c_str())) {                                                                   \
                    result = t;                                                                                         \
                }                                                                                                       \
            }
            BUTTON(MessageBoxButtons::Ok);
            BUTTON(MessageBoxButtons::Yes);
            BUTTON(MessageBoxButtons::Save);
            BUTTON(MessageBoxButtons::DontSave);
            BUTTON(MessageBoxButtons::No);
            BUTTON(MessageBoxButtons::Cancel);

            if (result.has_value()) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        return result;
    }
} // Carrot::Widgets