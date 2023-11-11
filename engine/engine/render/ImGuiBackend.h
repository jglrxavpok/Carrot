//
// Created by jglrxavpok on 02/11/2023.
//

#pragma once

#include <engine/Window.h>

struct ImDrawData;
struct ImGuiViewport;

namespace Carrot {
    class VulkanRenderer;

    namespace Render {
        struct Context;
        struct PImpl;
        struct ImGuiRendererData;

        /**
         * Implementation of an custom ImGui backend for Carrot.
         * Required because the default backend does not play nice with having a separate render thread
         * (render thread would not be able to record contents of ImDrawData while main thread is adding to the UI)
         */
        class ImGuiBackend {
        public:
            explicit ImGuiBackend(VulkanRenderer& renderer);
            ~ImGuiBackend();

            void initResources();
            void cleanup();

            void newFrame();

            /// Queues render packets to render the current ImGui windows, called from main thread
            void render(const Carrot::Render::Context& renderContext, WindowID windowID, ImDrawData* pDrawData);

            /// Record commands based on last call to 'render', called from render thread
            void record(vk::CommandBuffer& cmds, vk::RenderPass renderPass, const Carrot::Render::Context& renderContext);

            void onSwapchainImageCountChange(std::size_t newCount);

        public: // called by ImGui
            void createWindowImGui(ImGuiViewport* pViewport);
            void renderExternalWindowImGui(ImDrawData* pDrawData, ImGuiRendererData& externalWindow, std::size_t swapchainIndex);

        private:
            VulkanRenderer& renderer;
            PImpl* pImpl = nullptr;
        };

    }
} // Carrot::Render
