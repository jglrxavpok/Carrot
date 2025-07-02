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

#define VK_DISPATCHER_TYPE vk::detail::DispatchLoaderDynamic
#define VK_LOADER_TYPE vk::detail::DynamicLoader

namespace Fertilizer {
    struct GPUBuffer {
        vk::UniqueHandle<vk::DeviceMemory, VK_DISPATCHER_TYPE> vkMemory;
        vk::UniqueHandle<vk::Buffer, VK_DISPATCHER_TYPE> vkBuffer;
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

        VK_DISPATCHER_TYPE& getDispatcher();

        static vk::TransformMatrixKHR glmToRTTransformMatrix(const glm::mat4& mat);


    private:
        GPUBuffer newBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags memoryFlags);

        vk::PhysicalDevice findPhysicalDevice(VK_DISPATCHER_TYPE& dispatch);
        std::uint32_t findMemoryType(vk::PhysicalDevice device, std::uint32_t typeFilter, vk::MemoryPropertyFlags properties) const;

        VK_LOADER_TYPE dl;
        std::unique_ptr<VK_DISPATCHER_TYPE> dispatch;
        vk::UniqueHandle<vk::Instance, VK_DISPATCHER_TYPE> vkInstance;
        vk::PhysicalDevice vkPhysicalDevice;
        vk::UniqueHandle<vk::Device, VK_DISPATCHER_TYPE> vkDevice;
        vk::Queue vkComputeQueue;
        vk::UniqueHandle<vk::CommandPool, VK_DISPATCHER_TYPE> vkComputeCommandPool;
    };

} // Fertilizer
