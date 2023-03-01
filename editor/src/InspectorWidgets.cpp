//
// Created by jglrxavpok on 01/03/2023.
//

#include <Peeler.h>
#include <core/utils/ImGuiUtils.hpp>
#include <engine/edition/DragDropTypes.h>

namespace Peeler {
    constexpr const char* const PickWidgetTexture = "resources/textures/ui/picker.png";
    constexpr const char* const ResetWidgetTexture = "resources/textures/ui/reset.png";

    bool Application::drawPickEntityWidget(const char* label, Carrot::ECS::Entity& destination) {
        struct WidgetState {
            bool tryingToPick = false;
            std::uint32_t lastFrame = 0;
        };
        static std::unordered_map<std::string, WidgetState> states{};

        bool hasChanged = false;

        ImGui::PushID(label);
        CLEANUP(ImGui::PopID());

        float buttonSize = ImGui::GetTextLineHeight();

        std::string strLabel = std::string { label };
        WidgetState& state = states[strLabel];

        std::string entityName;
        bool hasEntity = false;
        if(destination == Carrot::UUID::null() || !currentScene.world.exists(destination)) {
            entityName = "<None>";
        } else {
            hasEntity = true;
            entityName = currentScene.world.getName(destination);
        }

        std::string text = strLabel.substr(0, strLabel.find("##"));
        ImGui::Text("%s", text.c_str());

        ImGui::SameLine();

        if(hasEntity) {
            auto texture = GetRenderer().getOrCreateTextureFullPath(ResetWidgetTexture);
            if (ImGui::ImageButton(texture->getImguiID(), ImVec2(buttonSize, buttonSize))) {
                destination = {Carrot::UUID::null(), currentScene.world};
                hasChanged = true;
            }
            ImGui::SameLine();
        }

        float l = ImGui::GetTextLineHeight();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvailWidth() - buttonSize - 2 * ImGui::GetStyle().CellPadding.x - ImGui::GetStyle().WindowPadding.x);
        ImGui::InputText("##entity name display", entityName, ImGuiInputTextFlags_ReadOnly);

        if(ImGui::BeginDragDropTarget()) {
            if(auto* payload = ImGui::AcceptDragDropPayload(Carrot::Edition::DragDropTypes::EntityUUID)) {
                verify(payload->DataSize == sizeof(Carrot::UUID), "Invalid payload for EntityUUID");
                static_assert(sizeof(Carrot::ECS::EntityID) == sizeof(Carrot::UUID), "Assumes EntityID = UUID");
                std::uint32_t* data = reinterpret_cast<std::uint32_t*>(payload->Data);
                Carrot::UUID uuid{data[0], data[1], data[2], data[3]};

                if(currentScene.world.exists(uuid)) {
                    destination = {uuid, currentScene.world};
                    hasChanged = true;
                }
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::SameLine();

        {
            auto texture = GetRenderer().getOrCreateTextureFullPath(PickWidgetTexture);
            if (state.lastFrame < GetRenderer().getFrameCount() - 1) {
                state.tryingToPick = false;
            }
            state.lastFrame = GetRenderer().getFrameCount();

            ImVec2 uv0{0, 0};
            ImVec2 uv1{1, 1};
            ImVec4 backgroundColor{0, 0, 0, 0};
            ImVec4 tintColor{1, 1, 1, 1};
            if (state.tryingToPick) {
                tintColor = ImVec4(0.5f, 1.0f, 0.5f, 1.0f);
            }

            if (ImGui::ImageButton(texture->getImguiID(), ImVec2(buttonSize, buttonSize), uv0, uv1, -1, backgroundColor,
                                   tintColor)) {
                state.tryingToPick = !state.tryingToPick;
            }
        }

        if(state.tryingToPick && entityIDPickedThisFrame != Carrot::UUID::null()) {
            destination = Carrot::ECS::Entity{ entityIDPickedThisFrame, currentScene.world };
            entityIDPickedThisFrame = Carrot::UUID::null();
            state.tryingToPick = false;
            hasChanged = true;
        }

        return hasChanged;
    }
}