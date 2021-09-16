//
// Created by jglrxavpok on 01/05/2021.
//

#pragma once

#include "engine/Engine.h"
#include "imgui.h"
#include "EditorSettings.h"
#include "imgui_node_editor.h"
#include "EditorGraph.h"
#include "TemplateEditor.h"
#include "ProjectMenuHolder.h"

namespace Tools {
    class ParticleEditor: public SwapchainAware, public ProjectMenuHolder {
    private:
        Carrot::Engine& engine;
        EditorSettings settings;
        EditorGraph updateGraph;
        EditorGraph renderGraph;
        TemplateEditor templateEditor;
        std::unique_ptr<Carrot::Render::Graph> previewRenderGraph = nullptr;
        std::unique_ptr<Carrot::Render::GraphBuilder> previewRenderGraphBuilder = nullptr;

        bool hasUnsavedChanges = false;

        void updateUpdateGraph(Carrot::Render::Context renderContext);
        void updateRenderGraph(Carrot::Render::Context renderContext);

    public:
        void clear();
        bool showUnsavedChangesPopup() override;

        void saveToFile(std::filesystem::path path) override;

        void performLoad(std::filesystem::path path) override;

    public:
        explicit ParticleEditor(Carrot::Engine& engine);

        void onFrame(Carrot::Render::Context renderContext);
        void tick(double deltaTime);

        ~ParticleEditor();

    public:
        static void addCommonInputs(Tools::EditorGraph& graph);
        static void addCommonOperators(Tools::EditorGraph& graph);
        static void addCommonLogic(Tools::EditorGraph& graph);
        static void addCommonMath(Tools::EditorGraph& graph);

    public:
        void onSwapchainImageCountChange(size_t newCount) override;

        void onSwapchainSizeChange(int newWidth, int newHeight) override;

    };
}
