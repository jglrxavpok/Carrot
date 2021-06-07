//
// Created by jglrxavpok on 02/06/2021.
//

#include "Console.h"
#include "imgui.h"
#include "RuntimeOption.hpp"
#include "engine/io/Logging.hpp"
#include "engine/utils/stringmanip.h"
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
        if(!visible)
            return;

        ImGuiWindowFlags flags = autocompleteField.getParentWindowFlags();

        if(ImGui::Begin("Console", nullptr, flags)) {
            autocompleteField.drawField();
        }
        ImGui::End();

        autocompleteField.drawAutocompleteSuggestions();
    }

    void Console::toggleVisibility() {
        visible = !visible;
    }

    Console& Console::instance() {
        static Console instance;
        return instance;
    }
}