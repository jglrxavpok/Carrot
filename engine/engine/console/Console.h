//
// Created by jglrxavpok on 02/06/2021.
//

#pragma once

#include <string>
#include <map>
#include <vector>
#include <functional>
#include "AutocompleteField.h"

namespace Carrot {
    class Engine;

    class Console {
    public:

        using CommandCallback = std::function<void(Carrot::Engine& engine)>; // TODO: support for arguments

        static Console& instance();

        void registerCommand(const std::string& name, CommandCallback callback);
        void registerCommands();
        void renderToImGui(Carrot::Engine& engine);
        void toggleVisibility();


    private:
        explicit Console();

        bool visible = false;
        std::map<std::string, CommandCallback> commands;
        Carrot::AutocompleteField autocompleteField;
        Carrot::Engine* engine = nullptr;
    };
}
