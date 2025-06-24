//
// Created by jglrxavpok on 01/05/2021.
//

#pragma once

#include "engine/Engine.h"
#include "imgui.h"
#include "engine/edition/EditorSettings.h"
#include "imgui_node_editor.h"
#include "../../../asset_tools/fertilizer/node_based/EditorGraph.h"
#include "TemplateEditor.h"
#include <engine/edition/ProjectMenuHolder.h>
#include <engine/render/Camera.h>
#include <engine/render/particles/Particles.h>
#include <engine/render/resources/BufferView.h>

namespace Peeler {
    class ParticleEditor: public SwapchainAware, public Tools::ProjectMenuHolder {
    public:
        enum class Graph {
            None,
            Render,
            Update,
        };

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
        void onCutShortcut(const Carrot::Render::Context& frame) override;
        void onCopyShortcut(const Carrot::Render::Context& frame) override;
        void onPasteShortcut(const Carrot::Render::Context& frame) override;
        void onDuplicateShortcut(const Carrot::Render::Context& frame) override;

    public:
        void onSwapchainImageCountChange(size_t newCount) override;

        void onSwapchainSizeChange(Carrot::Window& window, int newWidth, int newHeight) override;

    private:
        Carrot::Engine& engine;
        Graph focusedGraph;
        Tools::EditorSettings settings;
        Fertilizer::EditorGraph updateGraph;
        Fertilizer::EditorGraph renderGraph;
        double reloadCooldown = 0.0;
        std::chrono::time_point<std::chrono::steady_clock> lastReloadTime{};
        Peeler::TemplateEditor templateEditor;
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
