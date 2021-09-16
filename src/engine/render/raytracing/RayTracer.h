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
#include "engine/render/RenderGraph.h"

namespace Carrot {
    /// Class responsible for creating acceleration structures and updating them, creating the shader binding table and
    /// issuing raytracing operations
    /// Modeled on NVIDIA's tutorial: https://nvpro-samples.github.io/vk_raytracing_tutorial_KHR/
    class RayTracer: public SwapchainAware {
    private:
        VulkanRenderer& renderer;
        bool enabled = false;

        vk::PhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingProperties;

        vk::UniqueDescriptorPool rtDescriptorPool;
        vk::UniqueDescriptorPool sceneDescriptorPool;
        vk::UniqueDescriptorSetLayout rtDescriptorLayout;
        vk::UniqueDescriptorSetLayout sceneDescriptorLayout;
        std::unordered_map<Render::Eye, std::vector<vk::DescriptorSet>> rtDescriptorSets;
        std::vector<vk::DescriptorSet> sceneDescriptorSets;
        std::vector<vk::RayTracingShaderGroupCreateInfoKHR> shaderGroups;
        vk::UniquePipelineLayout pipelineLayout;
        vk::UniquePipeline pipeline;
        std::unique_ptr<Buffer> sbtBuffer;
        std::vector<std::unique_ptr<Buffer>> sceneBuffers{};
        std::unique_ptr<RaycastedShadowingLightBuffer> lightBuffer;
        std::vector<std::unique_ptr<Buffer>> lightVkBuffers{};
        std::size_t vertexBufferIndex = 0;
        std::size_t indexBufferIndex = 0;

        bool hasStuffToDraw = false;

        constexpr std::uint32_t alignUp(std::uint32_t value, std::uint32_t alignment);

        void createRTDescriptorSets();
        void createSceneDescriptorSets();

        void registerBuffer(std::uint32_t bindingIndex, const Buffer& vertexBuffer, vk::DeviceSize start, vk::DeviceSize length, std::size_t& index);

        void generateBuffers();

    public:
        /// Extensions required for raytracing
        static std::vector<const char*> getRequiredDeviceExtensions();

        explicit RayTracer(VulkanRenderer& renderer);

        void recordCommands(Render::Context renderContext, vk::CommandBuffer& commands);
        void onFrame(Render::Context renderContext);

        void onSwapchainRecreation();

        void createDescriptorSets();

        void createPipeline();

        void createShaderBindingTable();

        void init();
        void finishInit();

        RaycastedShadowingLightBuffer& getLightBuffer();

        void registerVertexBuffer(const Buffer& vertexBuffer, vk::DeviceSize start, vk::DeviceSize length);
        void registerIndexBuffer(const Buffer& indexBuffer, vk::DeviceSize start, vk::DeviceSize length);

        void onSwapchainImageCountChange(std::size_t newCount) override;

        void onSwapchainSizeChange(int newWidth, int newHeight) override;

        Carrot::Render::Pass<Carrot::Render::PassData::Raytracing>& appendRTPass(Carrot::Render::GraphBuilder& mainGraph, Carrot::Render::Eye eye);

    public:
        std::unordered_map<Render::Eye, std::vector<vk::DescriptorSet>>& getRTDescriptorSets() { return rtDescriptorSets; }
    };
}
