//
// Created by jglrxavpok on 29/12/2020.
//

#pragma once
#include "engine/vulkan/SwapchainAware.h"

namespace Carrot {
    namespace Render {
        struct Context;
    }

    class VulkanRenderer;

    /// Class responsible for creating acceleration structures & scene information buffers and updating them
    /// Modeled on NVIDIA's tutorial: https://nvpro-samples.github.io/vk_raytracing_tutorial_KHR/
    class RayTracer: public SwapchainAware {
    public:
        // TODO: scene buffers
        // TODO: mesh registration
        // TODO: mesh un-registration
        // TODO: Provide a descriptor set for rayqueries (AS + scene information)

        /// Extensions required for raytracing
        static std::vector<const char*> getRequiredDeviceExtensions();

        explicit RayTracer(VulkanRenderer& renderer);

        void onFrame(const Render::Context& renderContext);

        void onSwapchainImageCountChange(std::size_t newCount) override;

        void onSwapchainSizeChange(Window& window, int newWidth, int newHeight) override;

    private:
        VulkanRenderer& renderer;
        bool available = false;

        vk::PhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingProperties;
    };
}
