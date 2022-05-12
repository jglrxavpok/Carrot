//
// Created by jglrxavpok on 06/06/2021.
// based on https://gist.github.com/harold-b/7dcc02557c2b15d76c61fde1186e31d0
//

#pragma once

#include <imgui.h>
#include <string>
#include <functional>

namespace Carrot {

    // based on https://gist.github.com/harold-b/7dcc02557c2b15d76c61fde1186e31d0
    class AutocompleteField {
    public:
        explicit AutocompleteField(std::function<void(const std::string&)> callback, std::function<void(std::vector<std::string>&, const std::string&)> updateSuggestions);

        ImGuiWindowFlags getParentWindowFlags() const;

    public:
        void drawField();

        void drawAutocompleteSuggestions();

        struct State {
            bool isPopupOpen = false;
            int  activeIndex = -1;         // Index of currently 'active' item by use of up/down keys
            int  clickedIndex = -1;        // Index of popup item clicked with the mouse
            bool selectionChanged = false;  // Flag to help focus the correct item when selecting active item
        };

    public:
        std::string& getBuffer();

    public: // public only for interfacing with ImGui
        void refreshSuggestions();
        std::vector<std::string>& getCurrentSuggestions();
        State& getState();
        void setInputFromActiveIndex(ImGuiInputTextCallbackData* data, int entryIndex);

    private:

    private:
        State currentState;
        ImVec2 popupPosition;
        ImVec2 popupSize;
        bool isWindowFocused = false;

        std::function<void(const std::string&)> callback;
        std::function<void(std::vector<std::string>&, const std::string&)> updateSuggestions;

        std::vector<std::string> currentSuggestions;

        std::string imguiInputBuffer;
    };
}
