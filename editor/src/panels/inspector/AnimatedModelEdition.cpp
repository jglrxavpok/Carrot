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
        PreviewState state = PreviewState::Stopped;
        PreviewState previousState = PreviewState::Stopped;

        double lastUpdateTime = 0.0;
    };
    static std::unordered_map<ImGuiID, WidgetState> states{};

    void updatePreview(WidgetState& state, const Carrot::Vector<Carrot::ECS::AnimatedModelComponent*>& components) {
        const double currentTime = GetEngine().getCurrentFrameTime();
        double deltaTime = currentTime - state.lastUpdateTime;
        if(state.previousState != PreviewState::Playing) {
            deltaTime = 0.0;
        }
        if(state.state == PreviewState::Playing) {
            for(auto& c : components) {
                auto& instance = c->asyncAnimatedModelHandle->getData();
                instance.animationTime += deltaTime;
            }
        } else if(state.state == PreviewState::Stopped) {
            for(auto& c : components) {
                auto& instance = c->asyncAnimatedModelHandle->getData();
                instance.animationTime = 0.0f;
            }
        }

        state.lastUpdateTime = currentTime;
        state.previousState = state.state;
    }

    void editAnimatedModelComponent(EditContext& edition, const Carrot::Vector<Carrot::ECS::AnimatedModelComponent*>& components) {
        // check if all components have loaded their models
        for(std::int64_t i = 0; i < components.size(); i++) {
            auto& asyncModel = components[i]->asyncAnimatedModelHandle;
            if(!asyncModel.isReady() && !asyncModel.isEmpty()) {
                ImGui::Text("Loading model(s)...");
                return;
            }
        }

        ImGui::BeginGroup();
        CLEANUP({
            ImGui::EndGroup();
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
                        for(auto& pComponent : components) {
                            pComponent->queueLoad(vfsPath);
                        }
                        edition.hasModifications = true;
                    }
                }

                ImGui::EndDragDropTarget();
            }
        });

        const ImGuiID currentID = ImGui::GetID("Preview");

        WidgetState& state = states[currentID];

        multiEditField(edition, "Filepath", components,
            +[](Carrot::ECS::AnimatedModelComponent& c) {
                return Carrot::IO::VFS::Path { c.asyncAnimatedModelHandle.isEmpty() ? "" : c.asyncAnimatedModelHandle->getParent().getModel().getOriginatingResource().getName() };
            },
            +[](Carrot::ECS::AnimatedModelComponent& c, const Carrot::IO::VFS::Path& path) {
                c.queueLoad(path);
            },
            Helpers::Limits<Carrot::IO::VFS::Path> {
                .validityChecker = [](const auto& path) { return Carrot::IO::isModelFormat(path.toString().c_str()); }
            }
        );

        // check components actually have a model
        for(std::int64_t i = 0; i < components.size(); i++) {
            auto& asyncModel = components[i]->asyncAnimatedModelHandle;
            if(asyncModel.isEmpty()) {
                return;
            }
        }

        // check if all components use the same animated model
        Carrot::Render::AnimatedModel* pAnimatedModel = &components[0]->asyncAnimatedModelHandle->getParent();
        for(std::int64_t i = 1; i < components.size(); i++) {
            if(pAnimatedModel != &components[i]->asyncAnimatedModelHandle->getParent()) {
                return;
            }
        }

        const Carrot::Render::AnimatedModel& animatedModel = *pAnimatedModel;
        const Carrot::Model& baseModel = animatedModel.getModel();
        Carrot::Render::AnimatedModel::Handle& firstHandle = *components[0]->asyncAnimatedModelHandle.get();

        if(ImGui::CollapsingHeader("Preview")) {
            const Carrot::AnimationMetadata* selectedAnimation = baseModel.getAnimationMetadata(state.selectedAnimation);
            const std::string previewText = selectedAnimation ? state.selectedAnimation : "";
            const float lineHeight = ImGui::GetTextLineHeight();
            const ImVec2 buttonSize = ImVec2(lineHeight, lineHeight);
            const char* comboLabel = "Animation";
            const float labelWidth = ImGui::CalcTextSize(comboLabel, nullptr, true).x;
            const float comboWidth =
                    ImGui::GetContentRegionAvail().x // available width
                    - labelWidth // combo box size does not take label width into account
                    - ImGui::GetStyle().ItemInnerSpacing.x // space between combo and its label
                    - buttonSize.x * 4 // the 3 play buttons we want to add + some space to fit the last button?
                    - ImGui::GetStyle().ItemSpacing.x * 4 // spacing between buttons (2) + spacing between combo label and first button (1) + spacing against edge
                    ;

            ImGui::SetNextItemWidth(comboWidth);
            if(ImGui::BeginCombo(comboLabel, previewText.c_str())) {
                for(const auto& [animationName, animationData] : baseModel.getAnimationMetadata()) {
                    if(ImGui::Selectable(animationName.c_str(), firstHandle.getData().animationIndex == animationData.index)) {
                        firstHandle.getData().animationIndex = animationData.index;
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
            };

            auto pausePlaying = [&]() {
                state.state = PreviewState::Paused;
            };

            auto stopPlaying = [&]() {
                state.state = PreviewState::Stopped;
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

        updatePreview(state, components);
    }
}