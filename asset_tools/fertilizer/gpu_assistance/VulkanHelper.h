//
// Created by jglrxavpok on 30/07/2024.
//

#pragma once
#include <functional>
#include <glm/matrix.hpp>

#ifdef VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
    #if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
        #error This file should not be included inside any engine code!
    #endif
#endif

#undef VULKAN_HPP_DISABLE_ENHANCED_MODE
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 0
#define VULKAN_HPP_NO_DEFAULT_DISPATCHER
#include <memory>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_hash.hpp>

namespace Fertilizer {
    struct GPUBuffer {
        vk::UniqueHandle<vk::DeviceMemory, vk::detail::DispatchLoaderDynamic> vkMemory;
        vk::UniqueHandle<vk::Buffer, vk::detail::DispatchLoaderDynamic> vkBuffer;
    };

    /**
     * Helpers for GPU interfacing in the context of the asset pipeline
     * This is barebones and is not intended for rendering, only running basic GPU commands.
     * This is separate from the Carrot/engine code to be super simple to debug,
     * and mostly avoid a major refactoring of the engine structure to expose Vulkan wrapper objects
     */
    class VulkanHelper {
    public:
        /// Creates a new context and creates the necessary interfaces with Vulkan
        /// Reuse when possible
        VulkanHelper();

        void executeCommands(const std::function<void(vk::CommandBuffer)>& commandGenerator);
        GPUBuffer newHostVisibleBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage);
        GPUBuffer newDeviceLocalBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage);
        vk::Device getDevice();

        vk::detail::DispatchLoaderDynamic& getDispatcher();

        static vk::TransformMatrixKHR glmToRTTransformMatrix(const glm::mat4& mat);


    private:
        GPUBuffer newBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags memoryFlags);

        vk::PhysicalDevice findPhysicalDevice(vk::detail::DispatchLoaderDynamic& dispatch);
        std::uint32_t findMemoryType(vk::PhysicalDevice device, std::uint32_t typeFilter, vk::MemoryPropertyFlags properties) const;

        vk::detail::DynamicLoader dl;
        std::unique_ptr<vk::detail::DispatchLoaderDynamic> dispatch;
        vk::UniqueHandle<vk::Instance, vk::detail::DispatchLoaderDynamic> vkInstance;
        vk::PhysicalDevice vkPhysicalDevice;
        vk::UniqueHandle<vk::Device, vk::detail::DispatchLoaderDynamic> vkDevice;
        vk::Queue vkComputeQueue;
        vk::UniqueHandle<vk::CommandPool, vk::detail::DispatchLoaderDynamic> vkComputeCommandPool;
    };

} // Fertilizer
