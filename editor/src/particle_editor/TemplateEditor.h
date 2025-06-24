//
// Created by jglrxavpok on 28/05/2021.
//

#pragma once

#include <engine/Engine.h>
#include "imgui_node_editor.h"
#include "imgui.h"
#include <node_based/EditorGraph.h>
#include <engine/edition/EditorSettings.h>
#include <engine/edition/ProjectMenuHolder.h>

namespace Peeler {
    class TemplateEditor: public Tools::ProjectMenuHolder {
    public:
        TemplateEditor(Carrot::Engine& engine);

    public:
        void tick(double deltaTime);
        void onFrame(const Carrot::Render::Context& renderContext) override;
        void open();

        void performLoad(std::filesystem::path path) override;

        void saveToFile(std::filesystem::path path) override;

        bool showUnsavedChangesPopup() override;

    public:
        void onCutShortcut(const Carrot::Render::Context& frame) override;
        void onCopyShortcut(const Carrot::Render::Context& frame) override;
        void onPasteShortcut(const Carrot::Render::Context& frame) override;
        void onDuplicateShortcut(const Carrot::Render::Context& frame) override;

    public:
        Fertilizer::EditorGraph& getGraph() { return graph; };

    private:
        bool isOpen = false;
        bool hasUnsavedChanges = false;

        std::string menuLocation;
        char menuLocationImGuiBuffer[128];
        std::string title;
        char titleImGuiBuffer[128];
        Carrot::Engine& engine;
        Tools::EditorSettings settings;
        Fertilizer::EditorGraph graph;
    };
}
