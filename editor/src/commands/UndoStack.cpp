//
// Created by jglrxavpok on 18/03/2024.
//

#include "UndoStack.h"

#include <imgui/imgui.h>

namespace Peeler {
    ICommand::ICommand(Application& app, const std::string& _description): editor(app), description(_description) {}

    const std::string& ICommand::getDescription() const {
        return description;
    }

    UndoStack::UndoStack(Application& app): editor(app) {
    }

    void UndoStack::internalPush(Carrot::UniquePtr<ICommand> command) {
        stack.resize(cursor); // remove elements after the cursor
        cursor++;
        command->redo();
        stack.emplaceBack(std::move(command));
    }

    bool UndoStack::undo() {
        if(cursor <= 0) {
            return false;
        }
        stack[--cursor]->undo();
        return true;
    }

    bool UndoStack::redo() {
        if(cursor >= stack.size()) {
            return false;
        }
        stack[cursor++]->redo();
        return true;
    }

    void UndoStack::debugDraw() {
        if(ImGui::Begin("UndoStack debug")) {
            ImGui::Text("Cursor value: %lld", cursor);

            for(std::int64_t i = 0; i < stack.size(); i++) {
                if(i+1 == cursor) {
                    ImGui::Text("-> %s", stack[i]->getDescription().c_str());
                } else {
                    ImGui::Text("%s", stack[i]->getDescription().c_str());
                }
            }

            if(ImGui::SmallButton("Undo")) {
                undo();
            }

            ImGui::SameLine();

            if(ImGui::SmallButton("Redo")) {
                redo();
            }
        }
        ImGui::End();
    }
} // Peeler