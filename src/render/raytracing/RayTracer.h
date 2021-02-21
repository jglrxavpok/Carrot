//
// Created by jglrxavpok on 29/12/2020.
//

#pragma once
#include "Engine.h"
#include "vulkan/includes.h"
#include "RenderPasses.h"

namespace Carrot {
    /// Class responsible for creating acceleration structures and updating them, creating the shader binding table and
    /// issuing raytracing operations
    /// Modeled on NVIDIA's tutorial: https://nvpro-samples.github.io/vk_raytracing_tutorial_KHR/
    class RayTracer {
    private:
        Engine& engine;
        vk::PhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingProperties;
        vector<unique_ptr<Image>> lightingImages{};
        vector<vk::UniqueImageView> lightingImageViews{};

        vk::UniqueDescriptorPool rtDescriptorPool;
        vk::UniqueDescriptorSetLayout rtDescriptorLayout;
        vector<vk::DescriptorSet> rtDescriptorSets;
        vector<vk::RayTracingShaderGroupCreateInfoKHR> shaderGroups;
        vk::UniquePipelineLayout pipelineLayout;
        vk::UniquePipeline pipeline;
        unique_ptr<Buffer> sbtBuffer;

        void updateDescriptorSets();

        constexpr uint32_t alignUp(uint32_t value, uint32_t alignment);

    public:
        /// Extensions required for raytracing
        static vector<const char*> getRequiredDeviceExtensions();

        explicit RayTracer(Engine& engine);

        void recordCommands(uint32_t frameIndex, vk::CommandBuffer& commands);

        void onSwapchainRecreation();

        vector<vk::UniqueImageView>& getLightingImageViews();

        void createDescriptorSets();

        void createPipeline();

        void createShaderBindingTable();
    };
}
