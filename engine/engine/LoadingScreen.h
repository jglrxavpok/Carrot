//
// Created by jglrxavpok on 17/03/2021.
//

#pragma once
#include <memory>
#include "Engine.h"
#include "render/resources/Image.h"

namespace Carrot {
    class LoadingScreen {
    private:
        Engine& engine;
        std::unique_ptr<Image> loadingImage;

    public:
        explicit LoadingScreen(Engine& engine);

        ~LoadingScreen() = default;
    };
}

