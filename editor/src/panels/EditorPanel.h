//
// Created by jglrxavpok on 05/06/2022.
//

#pragma once

#include <engine/render/RenderContext.h>

namespace Peeler {
    class Application;

    class EditorPanel {
    public:
        EditorPanel(Peeler::Application& app): app(app) {}

        virtual void draw(const Carrot::Render::Context& renderContext) = 0;
        virtual ~EditorPanel() = default;

    protected:
        Peeler::Application& app;
    };
}