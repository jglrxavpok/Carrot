//
// Created by jglrxavpok on 02/06/2021.
//

#pragma once

namespace Carrot {
    class Engine;

    namespace Console {
        void registerCommands();
        void renderToImGui(Carrot::Engine& engine);
        void toggleVisibility();
    }
}
