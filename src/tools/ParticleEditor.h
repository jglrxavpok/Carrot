//
// Created by jglrxavpok on 01/05/2021.
//

#pragma once

#include "engine/Engine.h"
#include "imgui_node_editor.h"
#include "imgui.h"
#include "EditorGraph.h"
#include "EditorSettings.h"

namespace Tools {
    class ParticleEditor {
    private:
        Carrot::Engine& engine;
        EditorSettings settings;
        EditorGraph updateGraph;
        EditorGraph renderGraph;
        EditorGraph templateGraph;

        bool hasUnsavedChanges = false;
        bool tryingToOpenFile = false;
        std::filesystem::path fileToOpen;

        void updateUpdateGraph(size_t frameIndex);
        void updateRenderGraph(size_t frameIndex);

        void addCommonInputs(Tools::EditorGraph& graph);
        void addCommonOperators(Tools::EditorGraph& graph);
        void addCommonLogic(Tools::EditorGraph& graph);
        void addCommonMath(Tools::EditorGraph& graph);

        void performLoad();

    public:
        static std::filesystem::path EmptyProject;

        void clear();

        void scheduleLoad(std::filesystem::path path);
        void saveToFile(std::filesystem::path path);
        bool triggerSave();
        bool triggerSaveAs(std::filesystem::path& path);

    public:
        explicit ParticleEditor(Carrot::Engine& engine);

        void onFrame(size_t frameIndex);
        void tick(double deltaTime);

        ~ParticleEditor();
    };
}
