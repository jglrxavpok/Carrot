//
// Created by jglrxavpok on 22/09/2021.
//

#pragma once

#include <vector>
#include "RenderEye.h"
#include "Camera.h"
#include "engine/vulkan/SwapchainAware.h"
#include "engine/render/resources/BufferView.h"
#include "engine/render/resources/PerFrame.h"

namespace Carrot {
    class VulkanRenderer;
}

namespace Carrot::Render {
    class Graph;
    struct Context;

    class Viewport: public SwapchainAware {
    public:
        explicit Viewport(VulkanRenderer& renderer);
        Viewport(const Viewport&) = delete;

    public:
        //! Is this viewport expected to support dual eye rendering?
        bool vrCompatible = false;

    public: // camera
        Carrot::Camera& getCamera(Carrot::Render::Eye eye = Carrot::Render::Eye::NoVR);
        const Carrot::Camera& getCamera(Carrot::Render::Eye eye = Carrot::Render::Eye::NoVR) const;

        vk::DescriptorSet getCameraDescriptorSet(const Carrot::Render::Context& context) const;
        const Carrot::BufferView& getCameraUniformBuffer(const Render::Context& context) const;

        vk::DescriptorSet getViewportDescriptorSet(const Carrot::Render::Context& context) const;
        const Carrot::BufferView& getViewportUniformBuffer(const Render::Context& context) const;

    public:
        void onFrame(const Carrot::Render::Context& context);
        void render(const Carrot::Render::Context& context, vk::CommandBuffer& cmds);

    public:
        void resize(std::uint32_t width, std::uint32_t height);
        std::uint32_t getWidth() const;
        std::uint32_t getHeight() const;
        glm::vec2 getSizef() const;

        /// Offset of this viewport from top left of screen. Not handled automatically for now
        glm::vec2 getOffset() const;
        void setOffset(const glm::vec2& offset);

    public:
        void onSwapchainImageCountChange(size_t newCount) override;
        void onSwapchainSizeChange(int newWidth, int newHeight) override;

    public:
        /// Render graph associated to this viewport, may be nullptr
        Graph* getRenderGraph();
        void setRenderGraph(std::unique_ptr<Graph>&& renderGraph);

    private:
        VulkanRenderer& renderer;
        std::unique_ptr<Graph> renderGraph;
        bool followSwapchainSize = true;

        // used only if followSwapchainSize is false, otherwise getWidth/getHeight return the swapchain size
        std::uint32_t width = 100;
        std::uint32_t height = 100;
        glm::vec2 offset{0.0f};

        std::unordered_map<Render::Eye, Camera> cameras{};

        Render::PerFrame<Carrot::BufferView> cameraUniformBuffers;
        Render::PerFrame<vk::DescriptorSet> cameraDescriptorSets;

        Render::PerFrame<Carrot::BufferView> viewportUniformBuffers;
        Render::PerFrame<vk::DescriptorSet> viewportDescriptorSets;
    };
}
