//
// Created by jglrxavpok on 05/05/2026.
//

#include "Peeler.h"
#include <core/utils/ImGuiUtils.hpp>

namespace Peeler {
    void Application::drawViewportSettingsWindow() {
        if (showViewportSettings) {
            if (ImGui::Begin("Viewport settings", &showViewportSettings)) {
                if (ImGui::Button("Split screen")) {
                    Carrot::Render::ViewportComposition splitScreen2Players;
                    Carrot::Render::ViewportLocation& player0Loc = splitScreen2Players.viewports[Carrot::Identifier{"player_0"}];
                    player0Loc.offset = {0, 0};
                    player0Loc.size = {1.0f, 0.5f};

                    Carrot::Render::ViewportLocation& player1Loc = splitScreen2Players.viewports[Carrot::Identifier{"player_1"}];
                    player1Loc.offset = {0, 0.5f};
                    player1Loc.size = {1.0f, 0.5f};
                    gameTexture = updateViewportComposition(std::move(splitScreen2Players));
                }
                if (ImGui::Button("4-way split")) {
                    Carrot::Render::ViewportComposition splitScreen4Players;
                    Carrot::Render::ViewportLocation& player0Loc = splitScreen4Players.viewports[Carrot::Identifier{"player_0"}];
                    player0Loc.offset = {0, 0};
                    player0Loc.size = {0.5f, 0.5f};

                    Carrot::Render::ViewportLocation& player1Loc = splitScreen4Players.viewports[Carrot::Identifier{"player_1"}];
                    player1Loc.offset = {0.5f, 0.0f};
                    player1Loc.size = {0.5f, 0.5f};

                    Carrot::Render::ViewportLocation& player2Loc = splitScreen4Players.viewports[Carrot::Identifier{"player_2"}];
                    player2Loc.offset = {0.0f, 0.5f};
                    player2Loc.size = {0.5f, 0.5f};

                    Carrot::Render::ViewportLocation& player3Loc = splitScreen4Players.viewports[Carrot::Identifier{"player_3"}];
                    player3Loc.offset = {0.5f, 0.5f};
                    player3Loc.size = {0.5f, 0.5f};
                    gameTexture = updateViewportComposition(std::move(splitScreen4Players));
                }
                if (ImGui::Button("Single screen")) {
                    gameTexture = updateViewportComposition({});
                }

                ImGui::Separator();
                // preview
                ImVec2 previewSize = ImVec2(1920*0.5f, 1080*0.5f);
                ImDrawList* pDrawList = ImGui::GetWindowDrawList();
                {
                    ImVec2 startPos = ImGui::GetCursorScreenPos();

                    const float fontSize = 23.0f;
                    ImGui::PushFont(ImGui::GetFont(), fontSize);
                    CLEANUP(ImGui::PopFont());
                    for (const auto& [id, location] : currentComposition.viewports) {
                        const ImVec2 start = startPos + ImVec2(previewSize.x * location.offset.x, previewSize.y * location.offset.y);
                        const ImVec2 size(previewSize.x * location.size.x, previewSize.y * location.size.y);
                        const ImVec2 end = start + size;
                        ImColor color = ImGuiUtils::getDebugColorFromPaletteAsImColor(id.getHash());
                        color.Value.w = 0.5f;
                        pDrawList->AddRectFilled(start, end, color);
                        const std::string text {id};

                        const ImVec2 textSize = ImGui::CalcTextSize(text.c_str(), text.c_str() + text.size());

                        ImVec2 textPosition = start + size / 2 - textSize / 2; // center text

                        // shadow under text
                        pDrawList->AddText(ImGui::GetFont(), fontSize, textPosition + ImVec2(1, 1), ImColor(0.0f, 0.0f, 0.0f, 1.0f), text.c_str(), text.c_str()+text.size(), size.x);
                        pDrawList->AddText(ImGui::GetFont(), fontSize, textPosition, ImColor(1.0f, 1.0f, 1.0f, 1.0f), text.c_str(), text.c_str()+text.size(), size.x);
                    }
                }
                ImGui::Dummy(previewSize);
            }
            ImGui::End();
        }
    }
}