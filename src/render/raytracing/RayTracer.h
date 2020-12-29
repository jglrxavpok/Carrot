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
    class RayTracer {
    private:
        Engine& engine;
        vector<vk::CommandBuffer> commandBuffers{};
        vector<unique_ptr<Image>> lightingImages{};
        vector<vk::UniqueImageView> lightingImageViews{};

    public:
        explicit RayTracer(Engine& engine);

        void recordCommands(uint32_t frameIndex, vk::CommandBufferInheritanceInfo* inheritance);

        void onSwapchainRecreation();

        vector<vk::CommandBuffer>& getCommandBuffers();

        vector<vk::UniqueImageView>& getLightingImageViews();
    };
}
