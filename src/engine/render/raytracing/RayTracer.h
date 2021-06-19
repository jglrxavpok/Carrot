//
// Created by jglrxavpok on 29/12/2020.
//

#pragma once
#include "engine/vulkan/includes.h"
#include "engine/vulkan/SwapchainAware.h"
#include "engine/render/VulkanRenderer.h"
#include "engine/RenderPasses.h"
#include "engine/render/lighting/Lights.h"
#include "engine/render/resources/Texture.h"

namespace Carrot {
    /// Class responsible for creating acceleration structures and updating them, creating the shader binding table and
    /// issuing raytracing operations
    /// Modeled on NVIDIA's tutorial: https://nvpro-samples.github.io/vk_raytracing_tutorial_KHR/
    class RayTracer: public SwapchainAware {
    private:
        VulkanRenderer& renderer;
        vk::PhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingProperties;
        vector<unique_ptr<Render::Texture>> lightingTextures{};

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

        bool hasStuffToDraw = false;

        constexpr uint32_t alignUp(uint32_t value, uint32_t alignment);

        void createRTDescriptorSets();
        void createSceneDescriptorSets();

        void registerBuffer(uint32_t bindingIndex, const Buffer& vertexBuffer, vk::DeviceSize start, vk::DeviceSize length, size_t& index);

        void generateImages();
        void generateBuffers();

    public:
        /// Extensions required for raytracing
        static vector<const char*> getRequiredDeviceExtensions();

        explicit RayTracer(VulkanRenderer& renderer);

        void recordCommands(uint32_t frameIndex, vk::CommandBuffer& commands);

        void onSwapchainRecreation();

        vector<unique_ptr<Render::Texture>>& getLightingTextures();

        void createDescriptorSets();

        void createPipeline();

        void createShaderBindingTable();

        void init();
        void finishInit();

        RaycastedShadowingLightBuffer& getLightBuffer();

        void registerVertexBuffer(const Buffer& vertexBuffer, vk::DeviceSize start, vk::DeviceSize length);
        void registerIndexBuffer(const Buffer& indexBuffer, vk::DeviceSize start, vk::DeviceSize length);

        void onSwapchainImageCountChange(size_t newCount) override;

        void onSwapchainSizeChange(int newWidth, int newHeight) override;
    };
}
