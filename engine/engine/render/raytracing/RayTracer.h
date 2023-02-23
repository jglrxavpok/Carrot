//
// Created by jglrxavpok on 29/12/2020.
//

#pragma once
#include "engine/vulkan/SwapchainAware.h"
#include "engine/render/VulkanRenderer.h"
#include "engine/RenderPasses.h"
#include "engine/render/lighting/Lights.h"
#include "engine/render/resources/Texture.h"
#include "engine/render/RenderGraph.h"

namespace Carrot {
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

        void onFrame(Render::Context renderContext);

        void onSwapchainImageCountChange(std::size_t newCount) override;

        void onSwapchainSizeChange(int newWidth, int newHeight) override;

    private:
        VulkanRenderer& renderer;
        bool available = false;

        vk::PhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingProperties;
    };
}
