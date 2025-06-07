//
// Created by jglrxavpok on 01/05/2021.
//

#pragma once

#include "engine/Engine.h"
#include "imgui.h"
#include "engine/edition/EditorSettings.h"
#include "imgui_node_editor.h"
#include "EditorGraph.h"
#include "TemplateEditor.h"
#include <engine/edition/ProjectMenuHolder.h>
#include <engine/render/Camera.h>
#include <engine/render/particles/Particles.h>
#include <engine/render/resources/BufferView.h>

namespace Tools {
    class ParticleEditor: public SwapchainAware, public ProjectMenuHolder {
    public:
        void clear();
        bool showUnsavedChangesPopup() override;

        void saveToFile(std::filesystem::path path) override;

        void performLoad(std::filesystem::path path) override;

    public:
        explicit ParticleEditor(Carrot::Engine& engine);

        void onFrame(const Carrot::Render::Context& renderContext) override;
        void tick(double deltaTime);

        ~ParticleEditor();

    public:
        static void addCommonInputs(Tools::EditorGraph& graph);
        static void addCommonOperators(Tools::EditorGraph& graph);
        static void addCommonLogic(Tools::EditorGraph& graph);
        static void addCommonMath(Tools::EditorGraph& graph);

    public:
        void onSwapchainImageCountChange(size_t newCount) override;

        void onSwapchainSizeChange(Carrot::Window& window, int newWidth, int newHeight) override;

    private:
        Carrot::Engine& engine;
        EditorSettings settings;
        EditorGraph updateGraph;
        EditorGraph renderGraph;
        double reloadCooldown = 0.0;
        std::chrono::time_point<std::chrono::steady_clock> lastReloadTime{};
        TemplateEditor templateEditor;
        std::unique_ptr<Carrot::Render::Graph> previewRenderGraph = nullptr;
        std::unique_ptr<Carrot::Render::GraphBuilder> previewRenderGraphBuilder = nullptr;
        Carrot::Render::FrameResource previewColorTexture;
        std::vector<Carrot::BufferView> cameraUniformBufferViews;
        std::vector<vk::DescriptorSet> cameraDescriptorSets;
        Carrot::Render::Viewport& previewViewport;
        std::unique_ptr<Carrot::ParticleBlueprint> previewBlueprint = nullptr;
        std::unique_ptr<Carrot::ParticleSystem> previewSystem = nullptr;

        bool hasUnsavedChanges = false;

        void updateUpdateGraph(Carrot::Render::Context renderContext);
        void updateRenderGraph(Carrot::Render::Context renderContext);

        void UIParticlePreview(Carrot::Render::Context renderContext);
        void reloadPreview();

        void generateParticleFile(const std::filesystem::path& filename);

    };
}
