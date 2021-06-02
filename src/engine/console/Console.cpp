//
// Created by jglrxavpok on 02/06/2021.
//

#include "Console.h"
#include "imgui.h"
#include "RuntimeOption.hpp"
#include <string>
#include <functional>
#include <map>
#include <utility>

namespace Carrot::Console {
    using CommandCallback = std::function<void(Carrot::Engine& engine)>; // TODO: support for arguments

    static bool visible = false;
    static std::map<std::string, CommandCallback> commands;

    void registerCommand(const std::string& name, CommandCallback callback) {
        commands[name] = std::move(callback);
    }

    void registerCommands() {
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

    void renderToImGui(Carrot::Engine& engine) {
        if(!visible)
            return;
        if(ImGui::Begin("Console")) {
            for(const auto& [command, action] : commands) {
                if(ImGui::Selectable(command.c_str())) {
                    action(engine);
                }
            }
        }
        ImGui::End();
    }

    void toggleVisibility() {
        visible = !visible;
    }
}