//
// Created by jglrxavpok on 06/06/2021.
//

#include "AutocompleteField.h"

#include <utility>
#include "imgui_internal.h"
#include "imgui_stdlib.h"

namespace Carrot {

    //-----------------------------------------------------------
    int InputCallback( ImGuiInputTextCallbackData* data )
    {
        Carrot::AutocompleteField& field = *reinterpret_cast<Carrot::AutocompleteField*>( data->UserData );
        auto& state = field.getState();

        switch( data->EventFlag )
        {
            case ImGuiInputTextFlags_CallbackCompletion :
                if( state.isPopupOpen && state.activeIndex != -1 )
                {
                    // Tab was pressed, grab the item's text
                    field.setInputFromActiveIndex( state.activeIndex );
                }

                state.isPopupOpen       = false;
                state.activeIndex         = -1;
                state.clickedIndex        = -1;

                break;

            case ImGuiInputTextFlags_CallbackHistory :

                state.isPopupOpen = true;

                if( ! field.getCurrentSuggestions().empty()) {
                    if( data->EventKey == ImGuiKey_UpArrow && state.activeIndex > 0 )
                    {
                        state.activeIndex--;
                        state.selectionChanged = true;
                    }
                    else if( data->EventKey == ImGuiKey_DownArrow && state.activeIndex < (field.getCurrentSuggestions().size()-1) ) // TODO: modify to customise contents
                    {
                        state.activeIndex++;
                        state.selectionChanged = true;
                    }
                } else {
                    state.activeIndex = -1;
                    state.selectionChanged = true;
                }

                break;

            case ImGuiInputTextFlags_CallbackAlways: {
                if(state.activeIndex >= 0) {
                    int size = field.getCurrentSuggestions().size();
                    if(size == 0) {
                        state.activeIndex = -1;
                    } else {
                        state.activeIndex = std::max(0, std::min(size, state.activeIndex));
                    }
                }
            } break;

            case ImGuiInputTextFlags_CallbackCharFilter:
                if(!state.isPopupOpen) {
                    state.isPopupOpen = true;
                    state.activeIndex   = 0;
                    state.selectionChanged = true;
                    state.clickedIndex  = -1;
                }
                break;

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

    AutocompleteField::AutocompleteField(std::function<void(const std::string&)> callback,
                                         std::function<void(std::vector<std::string>&, const std::string&)> updateSuggestions): callback(std::move(callback)), updateSuggestions(std::move(updateSuggestions)) {

    }

    ImGuiWindowFlags AutocompleteField::getParentWindowFlags() const {
        return currentState.isPopupOpen ? ImGuiWindowFlags_::ImGuiWindowFlags_NoBringToFrontOnFocus : 0;
    }

    void AutocompleteField::setInputFromActiveIndex( int entryIndex ) {
        // TODO: modify to customise contents
        auto& entry = getCurrentSuggestions()[entryIndex];

        imguiInputBuffer = entry;
    }


    void AutocompleteField::drawField() {
        ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue    |
                                    ImGuiInputTextFlags_CallbackAlways      |
                                    ImGuiInputTextFlags_CallbackCharFilter  |
                                    ImGuiInputTextFlags_CallbackCompletion  |
                                    ImGuiInputTextFlags_CallbackHistory;

        auto prevInput = imguiInputBuffer;
        bool pressedEnter = ImGui::InputText("Command##autocomplete", &imguiInputBuffer, flags, InputCallback, this);
        if(prevInput != imguiInputBuffer) {
            refreshSuggestions();
        }
        if(pressedEnter) {
            ImGui::SetKeyboardFocusHere(-1);

            if (currentState.isPopupOpen && currentState.activeIndex != -1) {
                // This means that enter was pressed whilst a
                // the popup was open and we had an 'active' item.
                // So we copy the entry to the input buffer here
                imguiInputBuffer = currentSuggestions[currentState.activeIndex];
            } else {
                // Handle text input here
                callback(imguiInputBuffer);
                imguiInputBuffer.clear();
            }

            // Hide popup
            currentState.isPopupOpen = false;
            currentState.activeIndex = -1;
        } else if(imguiInputBuffer.length() > 0) {
            if(!currentState.isPopupOpen) {
                currentState.isPopupOpen = true;
                currentState.activeIndex = -1;
                currentState.clickedIndex = -1;
                currentState.selectionChanged = false;
            }
        }

        // Restore focus to the input box if we just clicked an item
        if( currentState.clickedIndex != -1 )
        {
            ImGui::SetKeyboardFocusHere( -1 );

            // NOTE: We do not reset the 'clickedIdx' here because
            // we want to let the callback handle it in order to
            // modify the buffer, therefore we simply restore keyboard input instead

            // The user has clicked an item, grab the item text
            setInputFromActiveIndex(currentState.clickedIndex);

            // Hide the popup
            currentState.isPopupOpen = false;
            currentState.activeIndex = -1;
            currentState.clickedIndex = -1;
        }

        // Get input box position, so we can place the popup under it
        popupPosition = ImGui::GetItemRectMin();

        // Based on Omar's developer console demo: Retain focus on the input box
        if( ImGui::IsAnyItemFocused() &&
            !ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked( 0 ) )
        {
            ImGui::SetKeyboardFocusHere( -1 );
        }

        // Grab the position for the popup
        popupSize = ImVec2( ImGui::GetItemRectSize().x-60,
                            ImGui::GetTextLineHeightWithSpacing() * 4 );
        popupPosition.y += ImGui::GetItemRectSize().y;

        isWindowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_::ImGuiFocusedFlags_RootWindow);
    }

    void AutocompleteField::drawAutocompleteSuggestions() {
        if( !currentState.isPopupOpen )
            return;

        ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, 0 );

        ImGuiWindowFlags flags =
                ImGuiWindowFlags_NoTitleBar          |
                ImGuiWindowFlags_NoResize            |
                ImGuiWindowFlags_NoMove              |
                ImGuiWindowFlags_HorizontalScrollbar |
                ImGuiWindowFlags_NoSavedSettings     |
                ImGuiWindowFlags_NoFocusOnAppearing  |
                0/*ImGuiWindowFlags_ShowBorders*/;


        ImGui::SetNextWindowPos ( popupPosition );
        ImGui::SetNextWindowSize( popupSize );
        ImGui::Begin( "autocomplete_popup", nullptr, flags );
        ImGui::PushAllowKeyboardFocus( false );

        for( int i = 0; i < currentSuggestions.size(); i++ )
        {
            // Track if we're drawing the active index so we
            // can scroll to it if it has changed
            bool isIndexActive = currentState.activeIndex == i;

            if( isIndexActive )
            {
                // Draw the currently 'active' item differently
                // ( used appropriate colors for your own style )
                ImGui::PushStyleColor( ImGuiCol_Border, ImVec4( 1, 0, 0, 1 ) );
            }

            ImGui::PushID( i );
            if( ImGui::Selectable( currentSuggestions[i].c_str(), isIndexActive ) )
            {
                // And item was clicked, notify the input
                // callback so that it can modify the input buffer
                currentState.clickedIndex = i;
            }
            ImGui::PopID();

            if( isIndexActive )
            {
                if( currentState.selectionChanged )
                {
                    // Make sure we bring the currently 'active' item into view.
                    ImGui::SetScrollHereX();
                    ImGui::SetScrollHereY();
                    currentState.selectionChanged = false;
                }

                ImGui::PopStyleColor(1);
            }
        }

        bool isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_::ImGuiFocusedFlags_RootWindow);

        ImGui::PopAllowKeyboardFocus();
        ImGui::End();
        ImGui::PopStyleVar(1);


        // If neither of the windows has focus, hide the popup
        if( !isWindowFocused && !isFocused )
        {
            currentState.isPopupOpen = false;
        }
    }

    void AutocompleteField::refreshSuggestions() {
        currentSuggestions.clear();
        updateSuggestions(currentSuggestions, imguiInputBuffer);

        currentState.isPopupOpen = true;
        currentState.activeIndex   = 0;
        currentState.selectionChanged = true;
        currentState.clickedIndex  = -1;
    }

    std::vector<std::string>& AutocompleteField::getCurrentSuggestions() {
        return currentSuggestions;
    }

    AutocompleteField::State& AutocompleteField::getState() {
        return currentState;
    }

    std::string& AutocompleteField::getBuffer() {
        return imguiInputBuffer;
    }

}
