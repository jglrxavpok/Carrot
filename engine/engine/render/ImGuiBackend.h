//
// Created by jglrxavpok on 02/11/2023.
//

#pragma once

struct ImDrawData;

namespace Carrot {
    class VulkanRenderer;

    namespace Render {
        struct Context;
        struct PImpl;

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
            void render(const Carrot::Render::Context& renderContext, ImDrawData* pDrawData);

            /// Record commands based on last call to 'render', called from render thread
            void record(vk::CommandBuffer& cmds, vk::RenderPass renderPass, const Carrot::Render::Context& renderContext);

            void onSwapchainImageCountChange(std::size_t newCount);

        private:
            VulkanRenderer& renderer;
            PImpl* pImpl = nullptr;
        };

    }
} // Carrot::Render
