//
// Created by jglrxavpok on 22/09/2021.
//

#pragma once

#include <vector>
#include "RenderEye.h"
#include "Camera.h"
#include "engine/vulkan/SwapchainAware.h"
#include "engine/render/resources/BufferView.h"

namespace Carrot {
    class VulkanRenderer;
}

namespace Carrot::Render {
    class Viewport: public SwapchainAware {
    public:
        explicit Viewport(VulkanRenderer& renderer);
        Viewport(const Viewport&) = delete;

    public: // camera
        Carrot::Camera& getCamera(Carrot::Render::Eye eye = Carrot::Render::Eye::NoVR);
        const Carrot::Camera& getCamera(Carrot::Render::Eye eye = Carrot::Render::Eye::NoVR) const;
        vk::DescriptorSet getCameraDescriptorSet(const Carrot::Render::Context& context) const;

    public:
        void onFrame(const Carrot::Render::Context& context);

    public:
        void onSwapchainImageCountChange(size_t newCount) override;
        void onSwapchainSizeChange(int newWidth, int newHeight) override;

    private:
        VulkanRenderer& renderer;
        // TODO: Render-graph

        std::unordered_map<Render::Eye, Camera> cameras{};
        std::vector<Carrot::BufferView> cameraUniformBuffers;
        std::vector<vk::DescriptorSet> cameraDescriptorSets;
    };
}
