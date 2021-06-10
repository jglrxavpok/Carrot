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
    class ParticleEditor: public ProjectMenuHolder {
    private:
        Carrot::Engine& engine;
        EditorSettings settings;
        EditorGraph updateGraph;
        EditorGraph renderGraph;
        TemplateEditor templateEditor;

        bool hasUnsavedChanges = false;

        void updateUpdateGraph(size_t frameIndex);
        void updateRenderGraph(size_t frameIndex);

    public:
        void clear();
        bool showUnsavedChangesPopup() override;

        void saveToFile(std::filesystem::path path) override;

        void performLoad(filesystem::path path) override;

    public:
        explicit ParticleEditor(Carrot::Engine& engine);

        void onFrame(size_t frameIndex);
        void tick(double deltaTime);

        ~ParticleEditor();

    public:
        static void addCommonInputs(Tools::EditorGraph& graph);
        static void addCommonOperators(Tools::EditorGraph& graph);
        static void addCommonLogic(Tools::EditorGraph& graph);
        static void addCommonMath(Tools::EditorGraph& graph);

    };
}
