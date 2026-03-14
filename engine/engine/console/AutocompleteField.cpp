//
// Created by jglrxavpok on 06/06/2021.
// based on https://gist.github.com/harold-b/7dcc02557c2b15d76c61fde1186e31d0
//

#include "AutocompleteField.h"

#include <imsearch.h>
#include <imgui_stdlib.h>
#include <imgui_internal.h>
#include <utility>
#include <core/containers/Vector.hpp>
#include <core/utils/stringmanip.h>
namespace ImStb {
#include <imstb_textedit.h>
}

namespace Carrot {

    AutocompleteField::AutocompleteField(std::function<void(const std::string&)> callback,
                                         std::function<void(std::vector<std::string>&, const std::string&)> updateSuggestions): callback(std::move(callback)), updateSuggestions(std::move(updateSuggestions)) {
        refreshSuggestions();
    }

    void AutocompleteField::drawField() {
        refreshSuggestions();
        if (ImGui::BeginCombo("Command", imguiInputBuffer.c_str())) {
            if (ImSearch::BeginSearch()) {
                bool enterPressed = ImGui::Shortcut(ImGuiKey_Enter, ImGuiInputFlags_RouteOverActive);
                ImSearch::SearchBar();

                bool searchBarFocused = ImGui::IsItemActive();

                Carrot::Vector<std::string> activeSuggestions;
                i32 index = 0;
                for (const std::string& suggestion : currentSuggestions) {
                    ImSearch::SearchableItem(suggestion.c_str(), [&](const char* n) {
                        bool isSelected = index == currentState.activeIndex;
                        if (ImGui::Selectable(n, isSelected)) {
                            applySelection(n);
                        }
                        activeSuggestions.pushBack(n);
                        index++;
                    });
                }
                ImSearch::EndSearch();

                if (searchBarFocused) {
                    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
                        currentState.activeIndex++;
                    }
                    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
                        if (currentState.activeIndex < 0) {
                            currentState.activeIndex = activeSuggestions.size()-1;
                        } else {
                            currentState.activeIndex--;
                            while (currentState.activeIndex < 0) {
                                currentState.activeIndex = activeSuggestions.size() + currentState.activeIndex - 1;
                            }
                        }
                    }
                    if (enterPressed) {
                        if (currentState.activeIndex >= 0 && currentState.activeIndex < activeSuggestions.size()) {
                            applySelection(activeSuggestions[currentState.activeIndex]);
                        }
                    }
                }
                if (activeSuggestions.empty()) {
                    currentState.activeIndex = -1;
                } else {
                    currentState.activeIndex %= activeSuggestions.size();
                }
            }

            ImGui::EndCombo();
        }
    }

    void AutocompleteField::applySelection(const std::string& selection) {
        imguiInputBuffer = selection;
        callback(selection);
        currentState.activeIndex = -1;
    }

    void AutocompleteField::refreshSuggestions() {
        currentSuggestions.clear();
        updateSuggestions(currentSuggestions, imguiInputBuffer);
    }

}
