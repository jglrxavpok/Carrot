//
// Created by jglrxavpok on 08/10/2023.
//

#include "EditorFunctions.h"
#include <engine/Engine.h>
#include <engine/ecs/components/AnimatedModelComponent.h>
#include <engine/edition/DragDropTypes.h>

namespace Peeler {
    static const char* PlayButtonIcons[2] = { "resources/textures/ui/play_button.png", "resources/textures/ui/play_button_playing.png" };
    static const char* PauseButtonIcons[2] = { "resources/textures/ui/pause_button.png", "resources/textures/ui/pause_button_paused.png" };
    static const char* StopButtonIcons[2] = { "resources/textures/ui/stop_button.png", "resources/textures/ui/stop_button.png" };

    enum class PreviewState {
        Stopped,
        Playing,
        Paused,
    };

    struct WidgetState {
        std::string selectedAnimation;
        float animationTime = 0.0f;
        PreviewState state = PreviewState::Stopped;
        PreviewState previousState = PreviewState::Stopped;

        double lastUpdateTime = 0.0;
    };
    static std::unordered_map<ImGuiID, WidgetState> states{};

    void updatePreview(WidgetState& state, Carrot::AnimatedInstanceData& instance) {
        const double currentTime = GetEngine().getCurrentFrameTime();
        double deltaTime = currentTime - state.lastUpdateTime;
        if(state.previousState != PreviewState::Playing) {
            deltaTime = 0.0;
        }
        if(state.state == PreviewState::Playing) {
            instance.animationTime += deltaTime;
        }

        state.lastUpdateTime = currentTime;
        state.previousState = state.state;
    }

    void editAnimatedModelComponent(EditContext& edition, Carrot::ECS::AnimatedModelComponent* component) {
        const ImGuiID currentID = ImGui::GetID("Preview");

        WidgetState& state = states[currentID];

        auto& asyncModel = component->asyncAnimatedModelHandle;
        if(!asyncModel.isReady() && !asyncModel.isEmpty()) {
            ImGui::Text("Model is loading...");
            return;
        }
        std::string path = asyncModel.isEmpty() ? "" : component->waitLoadAndGetOriginatingResource().getName();
        if(ImGui::InputText("Filepath##AnimatedModelComponent filepath inspector", path, ImGuiInputTextFlags_EnterReturnsTrue)) {
            component->queueLoad(Carrot::IO::VFS::Path(path));
            edition.hasModifications = true;
        }

        if(ImGui::BeginDragDropTarget()) {
            if(auto* payload = ImGui::AcceptDragDropPayload(Carrot::Edition::DragDropTypes::FilePath)) {
                std::unique_ptr<char8_t[]> buffer = std::make_unique<char8_t[]>(payload->DataSize+sizeof(char8_t));
                std::memcpy(buffer.get(), static_cast<const char8_t*>(payload->Data), payload->DataSize);
                buffer.get()[payload->DataSize] = '\0';

                std::u8string str = buffer.get();
                std::string s = Carrot::toString(str);

                auto vfsPath = Carrot::IO::VFS::Path(s);

                // TODO: no need to go through disk again
                std::filesystem::path fsPath = GetVFS().resolve(vfsPath);
                if(!std::filesystem::is_directory(fsPath) && Carrot::IO::isModelFormatFromPath(s.c_str())) {
                    component->queueLoad(vfsPath);
                    edition.hasModifications = true;
                }
            }

            ImGui::EndDragDropTarget();
        }

        auto& handle = *asyncModel.get();
        auto& animatedModel = handle.getParent().getModel();

        if(ImGui::CollapsingHeader("Preview")) {
            // TODO: play, pause, stop buttons
            // TODO: ability to disable preview=> stop
            const Carrot::AnimationMetadata* selectedAnimation = animatedModel.getAnimationMetadata(state.selectedAnimation);
            const std::string previewText = selectedAnimation ? state.selectedAnimation : "";
            const float lineHeight = ImGui::GetTextLineHeight();
            const ImVec2 buttonSize = ImVec2(lineHeight, lineHeight);
            const char* comboLabel = "Animation##AnimatedModelComponent preview combo";
            const float labelWidth = ImGui::CalcTextSize(comboLabel, NULL, true).x;
            const float comboWidth =
                    ImGui::GetContentRegionAvail().x // available width
                    - labelWidth // combo box size does not take label width into account
                    - ImGui::GetStyle().ItemInnerSpacing.x // space between combo and its label
                    - buttonSize.x * 4 // the 3 play buttons we want to add + some space to fit the last button?
                    - ImGui::GetStyle().ItemSpacing.x * 4 // spacing between buttons (2) + spacing between combo label and first button (1) + spacing against edge
                    ;

            ImGui::SetNextItemWidth(comboWidth);
            if(ImGui::BeginCombo(comboLabel, previewText.c_str())) {
                for(const auto& [animationName, animationData] : animatedModel.getAnimationMetadata()) {
                    if(ImGui::Selectable(animationName.c_str(), handle.getData().animationIndex == animationData.index)) {
                        handle.getData().animationIndex = animationData.index;
                        state.selectedAnimation = animationName;
                    }
                }

                ImGui::EndCombo();
            }

            ImGui::SameLine();
            const bool enablePlayButtons = selectedAnimation != nullptr;
            ImGui::BeginDisabled(!enablePlayButtons);

            auto startPlaying = [&]() {
                if(state.state == PreviewState::Playing) {
                    return;
                }
                state.state = PreviewState::Playing;
                state.animationTime = 0.0f;
            };

            auto pausePlaying = [&]() {
                state.state = PreviewState::Paused;
            };

            auto stopPlaying = [&]() {
                state.state = PreviewState::Stopped;
                state.animationTime = 0.0f;
            };

            auto icon = [&](const char* texturePaths[2], bool active) {
                if(active) {
                    return GetAssetServer().blockingLoadTexture(texturePaths[1])->getImguiID();
                } else {
                    return GetAssetServer().blockingLoadTexture(texturePaths[0])->getImguiID();
                }
            };

            if(ImGui::ImageButton(icon(PlayButtonIcons, state.state == PreviewState::Playing), buttonSize)) {
                startPlaying();
            }
            ImGui::SameLine();

            if(ImGui::ImageButton(icon(PauseButtonIcons, state.state == PreviewState::Paused), buttonSize)) {
                pausePlaying();
            }
            ImGui::SameLine();

            if(ImGui::ImageButton(icon(StopButtonIcons, state.state == PreviewState::Stopped), buttonSize)) {
                stopPlaying();
            }
            ImGui::EndDisabled();
        }

        updatePreview(state, handle.getData());
    }
}