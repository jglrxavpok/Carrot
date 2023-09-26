//
// Created by jglrxavpok on 01/03/2023.
//

#include <Peeler.h>
#include <core/utils/ImGuiUtils.hpp>
#include <engine/edition/DragDropTypes.h>
#include <engine/render/resources/Pipeline.h>
#include <panels/InspectorPanel.h>
#include <core/io/Logging.hpp>
#include <engine/assets/AssetServer.h>

// TODO: asset browser

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
            auto texture = GetAssetServer().blockingLoadTexture(ResetWidgetTexture);
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
            auto texture = GetAssetServer().blockingLoadTexture(PickWidgetTexture);
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

    bool InspectorPanel::drawPickEntityWidget(const char* label, Carrot::ECS::Entity* destination) { // re-declared in InspectorPanel for consistency
        return app.drawPickEntityWidget(label, *destination);
    }

    bool InspectorPanel::drawPickTextureWidget(const char* label, Carrot::Render::Texture::Ref* pOut, bool allowNull) {
        const ImGuiID currentID = ImGui::GetID(label);

        struct WidgetState {
            bool init = false;
            std::string textInputContents;
        };
        static std::unordered_map<ImGuiID, WidgetState> states{};

        bool updated = false;
        WidgetState& state = states[currentID];

        if(!state.init) {
            if(*pOut) {
                const Carrot::IO::Resource& source = pOut->get()->getOriginatingResource();
                if(source.isFile()) {
                    state.textInputContents = source.getName();
                }
            }

            state.init = true;
        }

        if(ImGui::InputText(label, state.textInputContents, ImGuiInputTextFlags_EnterReturnsTrue)) {
            if(state.textInputContents.empty() && allowNull) {
                *pOut = nullptr;
                updated = true;
            } else {
                auto textureRef = GetAssetServer().blockingLoadTexture(Carrot::IO::VFS::Path { state.textInputContents });
                *pOut = textureRef;
                updated = true;
            }
        }
        if(ImGui::BeginDragDropTarget()) {
            if(auto* payload = ImGui::AcceptDragDropPayload(Carrot::Edition::DragDropTypes::FilePath)) {
                std::unique_ptr<char8_t[]> buffer = std::make_unique<char8_t[]>(payload->DataSize+1);
                std::memcpy(buffer.get(), static_cast<const void*>(payload->Data), payload->DataSize);
                buffer.get()[payload->DataSize] = '\0';

                std::u8string newPath = buffer.get();

                std::filesystem::path fsPath = std::filesystem::proximate(newPath, std::filesystem::current_path());
                if(!std::filesystem::is_directory(fsPath) && Carrot::IO::isImageFormatFromPath(fsPath)) {
                    *pOut = GetAssetServer().blockingLoadTexture(fsPath.string().c_str());
                    updated = true;
                }
            }

            ImGui::EndDragDropTarget();
        }

        return updated;
    }

    bool InspectorPanel::drawPickPipelineWidget(const char* label, std::shared_ptr<Carrot::Pipeline>* pOut, bool allowNull) {
        const ImGuiID currentID = ImGui::GetID(label);

        struct WidgetState {
            bool init = false;
            std::string textInputContents;
        };
        static std::unordered_map<ImGuiID, WidgetState> states{};

        bool updated = false;
        WidgetState& state = states[currentID];

        // fill pipeline name
        if(!state.init) {
            if(*pOut) {
                const auto& source = (*pOut)->getDescription().originatingResource;
                if(source.isFile()) {
                    state.textInputContents = source.getName();
                    state.init = true;
                }
            }
        }

        if(ImGui::InputText(label, state.textInputContents, ImGuiInputTextFlags_EnterReturnsTrue)) {
            if(state.textInputContents.empty() && allowNull) {
                *pOut = nullptr;
                updated = true;
            } else {
                try {
                    *pOut = GetRenderer().getOrCreatePipelineFullPath(state.textInputContents);
                    updated = true;
                } catch(std::exception& e) {
                    Carrot::Log::error("Failed to load pipeline '%s': %s", state.textInputContents.c_str(), e.what());
                }
            }
        }
        return updated;
    }
}