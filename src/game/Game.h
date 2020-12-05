//
// Created by jglrxavpok on 05/12/2020.
//

#pragma once

namespace Carrot {
    class Engine;

    class Game {
    private:
        Engine& engine;

    public:
        explicit Game(Engine& engine);
    };
}
