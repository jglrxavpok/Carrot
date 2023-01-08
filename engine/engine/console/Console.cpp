//
// Created by jglrxavpok on 02/06/2021.
//

#include "Console.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "RuntimeOption.hpp"
#include "core/io/Logging.hpp"
#include "core/utils/stringmanip.h"
#include <utility>

namespace Carrot {

    static char commandBuffer[512] = { '\0' };

    Console::Console(): autocompleteField(
            // callback
            [this](const std::string& command) {
                auto it = commands.find(command);
                if(it != commands.end()) {
                    Carrot::Log::info("Executing command: %s", command.c_str());
                    it->second(*engine);
                } else {
                    Carrot::Log::error("Invalid command: %s", command.c_str());
                }
            },

            // suggestions
            [this](std::vector<std::string>& out, const std::string& currentInput) {
                for(const auto& [str, _] : commands) {
                    if(Carrot::toLowerCase(str).find(Carrot::toLowerCase(currentInput)) != std::string::npos) {
                        out.emplace_back(str);
                    }
                }
            }) {}

    void Console::registerCommand(const std::string& name, CommandCallback callback) {
        commands[name] = std::move(callback);
    }

    void Console::registerCommands() {
        for(auto& option : Carrot::RuntimeOption::getAllOptions()) {
            auto activate = [option](auto& engine) {
                option->setValue(true);
            };
            auto deactivate = [option](auto& engine) {
                option->setValue(false);
            };
            registerCommand("Activate "+option->getName(), activate);
            registerCommand("Deactivate "+option->getName(), deactivate);
        }
    }

    void Console::renderToImGui(Carrot::Engine& engine) {
        this->engine = &engine;
        if(!visible) {
            wasVisibleLastFrame = false;
            return;
        }

        ImGuiWindowFlags flags = autocompleteField.getParentWindowFlags() | ImGuiWindowFlags_::ImGuiWindowFlags_NoCollapse;

        const bool suggestionsStoleFocus = autocompleteField.drawAutocompleteSuggestions();

        if(ImGui::Begin("Console", &visible, flags)) {
            if(ImGui::BeginChild("Messages", ImVec2( 0, -ImGui::GetTextLineHeightWithSpacing() * 1.5f))) {
                auto tableFlags = ImGuiTableFlags_::ImGuiTableFlags_ScrollX
                        | ImGuiTableFlags_::ImGuiTableFlags_ScrollY
                        | ImGuiTableFlags_::ImGuiTableFlags_Borders
                  // TODO     | ImGuiTableFlags_::ImGuiTableFlags_Sortable
                        | ImGuiTableFlags_::ImGuiTableFlags_Reorderable
                        | ImGuiTableFlags_::ImGuiTableFlags_SizingFixedFit
                        ;
                if(ImGui::BeginTable("Messages table", 3, tableFlags)) {
                    ImGui::TableSetupColumn("Severity");
                    ImGui::TableSetupColumn("Timestamp");
                    ImGui::TableSetupColumn("Message");
                    ImGui::TableHeadersRow();

                    ImGuiListClipper clipper;
                    clipper.Begin(100);
                    const auto& messageList = Carrot::Log::getMessages();

                    while(clipper.Step()) {
                        auto messageIterator = messageList.begin();
                        for (int index = 0; index < clipper.DisplayStart; index++) {
                            messageIterator++;
                        }

                        for (int index = clipper.DisplayStart; index < clipper.DisplayEnd && messageIterator != messageList.end(); index++) {
                            const auto& message = *messageIterator;
                            messageIterator++;
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImColor color = ImColor(1.0f, 1.0f, 1.0f, 1.0f);
                            switch(message.severity) {
                                case Log::Severity::Warning:
                                    color = ImColor(1.0f, 1.0f, 0.0f, 1.0f);
                                    break;
                                case Log::Severity::Error:
                                    color = ImColor(1.0f, 0.0f, 0.0f, 1.0f);
                                    break;
                                case Log::Severity::Debug:
                                    color = ImColor(0.25f, 0.75f, 0.0f, 1.0f);
                                    break;
                            }
                            ImGui::PushStyleColor(ImGuiCol_::ImGuiCol_Text, color.Value);
                            ImGui::Text("%s", Carrot::Log::getSeverityString(message.severity));
                            ImGui::TableNextColumn();
                            ImGui::Text("%llu", message.timestamp);
                            ImGui::TableNextColumn();
                            ImGui::Text("%s", message.message.c_str());
                            ImGui::PopStyleColor();
                        }
                    }

                    ImGui::EndTable();
                }
            }
            ImGui::EndChild();

            {
                autocompleteField.drawField();
                if(suggestionsStoleFocus || !wasVisibleLastFrame) {
                    ImGui::SetKeyboardFocusHere(-1);
                }
            }
        }
        ImGui::End();

        wasVisibleLastFrame = visible;
    }

    void Console::toggleVisibility() {
        visible = !visible;
    }

    Console& Console::instance() {
        static Console instance;
        return instance;
    }
}