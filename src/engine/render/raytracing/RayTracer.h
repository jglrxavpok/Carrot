//
// Created by jglrxavpok on 29/12/2020.
//

#pragma once
#include "engine/vulkan/includes.h"
#include "engine/render/VulkanRenderer.h"
#include "engine/RenderPasses.h"
#include "engine/render/lighting/Lights.h"

namespace Carrot {
    /// Class responsible for creating acceleration structures and updating them, creating the shader binding table and
    /// issuing raytracing operations
    /// Modeled on NVIDIA's tutorial: https://nvpro-samples.github.io/vk_raytracing_tutorial_KHR/
    class RayTracer {
    private:
        VulkanRenderer& renderer;
        vk::PhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingProperties;
        vector<unique_ptr<Image>> lightingImages{};
        vector<vk::UniqueImageView> lightingImageViews{};

        vk::UniqueDescriptorPool rtDescriptorPool;
        vk::UniqueDescriptorPool sceneDescriptorPool;
        vk::UniqueDescriptorSetLayout rtDescriptorLayout;
        vk::UniqueDescriptorSetLayout sceneDescriptorLayout;
        vector<vk::DescriptorSet> rtDescriptorSets;
        vector<vk::DescriptorSet> sceneDescriptorSets;
        vector<vk::RayTracingShaderGroupCreateInfoKHR> shaderGroups;
        vk::UniquePipelineLayout pipelineLayout;
        vk::UniquePipeline pipeline;
        unique_ptr<Buffer> sbtBuffer;
        vector<unique_ptr<Buffer>> sceneBuffers{};
        unique_ptr<RaycastedShadowingLightBuffer> lightBuffer;
        vector<unique_ptr<Buffer>> lightVkBuffers{};
        size_t vertexBufferIndex = 0;
        size_t indexBufferIndex = 0;

        void updateDescriptorSets();

        constexpr uint32_t alignUp(uint32_t value, uint32_t alignment);

        void createRTDescriptorSets();
        void createSceneDescriptorSets();

        void registerBuffer(uint32_t bindingIndex, const Buffer& vertexBuffer, vk::DeviceSize start, vk::DeviceSize length, size_t& index);



    public:
        /// Extensions required for raytracing
        static vector<const char*> getRequiredDeviceExtensions();

        explicit RayTracer(VulkanRenderer& renderer);

        void recordCommands(uint32_t frameIndex, vk::CommandBuffer& commands);

        void onSwapchainRecreation();

        vector<vk::UniqueImageView>& getLightingImageViews();

        void createDescriptorSets();

        void createPipeline();

        void createShaderBindingTable();

        void finishInit();

        RaycastedShadowingLightBuffer& getLightBuffer();

        void registerVertexBuffer(const Buffer& vertexBuffer, vk::DeviceSize start, vk::DeviceSize length);
        void registerIndexBuffer(const Buffer& indexBuffer, vk::DeviceSize start, vk::DeviceSize length);
    };
}
