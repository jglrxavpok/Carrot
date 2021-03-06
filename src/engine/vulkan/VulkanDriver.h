//
// Created by jglrxavpok on 11/03/2021.
//

#pragma once

#include "includes.h"
#include <optional>
#include <vector>
#include <set>
#include <engine/memory/NakedPtr.hpp>
#include <GLFW/glfw3.h>
#include "engine/vulkan/SwapchainAware.h"
#include "engine/memory/ThreadLocal.hpp"

namespace Carrot {
    namespace Render {
        class Texture;
        class TextureRepository;
    };

    using namespace std;

    class Image;
    class Buffer;

    struct QueueFamilies {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;
        std::optional<uint32_t> transferFamily;
        std::optional<uint32_t> computeFamily;

        bool isComplete() const;
    };

    struct SwapChainSupportDetails {
        vk::SurfaceCapabilitiesKHR capabilities;
        std::vector<vk::SurfaceFormatKHR> formats;
        std::vector<vk::PresentModeKHR> presentModes;
    };

    class VulkanDriver: public SwapchainAware {
    private:
        const vk::AllocationCallbacks* allocator = nullptr;

        NakedPtr<GLFWwindow> window = nullptr;
        int framebufferWidth;
        int framebufferHeight;

        vk::UniqueInstance instance;
        vk::UniqueDebugUtilsMessengerEXT callback{};
        vk::PhysicalDevice physicalDevice{};
        vk::UniqueDevice device{};
        QueueFamilies queueFamilies{};
        vk::Queue graphicsQueue{};
        vk::Queue presentQueue{};
        vk::Queue transferQueue{};
        vk::Queue computeQueue{};
        vk::SurfaceKHR surface{};

        ThreadLocal<vk::UniqueCommandPool> graphicsCommandPool;
        ThreadLocal<vk::UniqueCommandPool> transferCommandPool;
        ThreadLocal<vk::UniqueCommandPool> computeCommandPool;

        std::shared_ptr<Render::Texture> defaultTexture = nullptr;

        vk::UniqueSampler linearRepeatSampler{};
        vk::UniqueSampler nearestRepeatSampler{};

        vk::UniqueSwapchainKHR swapchain{};
        vk::Format swapchainImageFormat = vk::Format::eUndefined;
        vk::Format depthFormat = vk::Format::eUndefined;
        vk::Extent2D swapchainExtent{};
        vector<std::shared_ptr<Render::Texture>> swapchainTextures{}; // will not own data because deleted with swapchain

        vector<shared_ptr<Buffer>> cameraUniformBuffers{};
        vector<shared_ptr<Buffer>> debugUniformBuffers{};
        std::unique_ptr<Render::TextureRepository> textureRepository = nullptr;

        /// Create Vulkan instance
        void createInstance();

        /// Check validation layers are supported (if NO_DEBUG disabled)
        bool checkValidationLayerSupport();

        /// Generate a vector with the required extensions for this application
        std::vector<const char *> getRequiredExtensions();

        /// Setups debug messages
        void setupDebugMessenger();

        /// Prepares a debug messenger
        void setupMessenger(vk::DebugUtilsMessengerCreateInfoEXT &ext);

        /// Select a GPU
        void pickPhysicalDevice();

        /// Rank physical device based on their capabilities
        int ratePhysicalDevice(const vk::PhysicalDevice& device);

        /// Create the logical device to interface with Vulkan
        void createLogicalDevice();

        /// Check the given device supports the extensions inside VULKAN_DEVICE_EXTENSIONS (constants.h)
        bool checkDeviceExtensionSupport(const vk::PhysicalDevice& logicalDevice);

        /// Create the rendering surface for Vulkan
        void createSurface();

        /// Create the command pool for transfer operations
        vk::UniqueCommandPool createTransferCommandPool();

        /// Create the command pool for graphic operations
        vk::UniqueCommandPool createGraphicsCommandPool();

        /// Create the command pool for compute operations
        vk::UniqueCommandPool createComputeCommandPool();

        void createDefaultTexture();

        /// Choose best surface format from the list of given formats
        /// \param formats
        /// \return
        vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const vector<vk::SurfaceFormatKHR>& formats);

        /// Choose best present mode from the list of given modes
        /// \param presentModes
        /// \return
        vk::PresentModeKHR chooseSwapPresentMode(const vector<vk::PresentModeKHR>& presentModes);

        /// Choose resolution of swap chain images
        vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities);

    public:
        VulkanDriver(NakedPtr<GLFWwindow> window);

        ~VulkanDriver();

        /// Gets the queue indices of a given physical device
        QueueFamilies findQueueFamilies(const vk::PhysicalDevice& device);

        vk::Device& getLogicalDevice() { return *device; };

        vk::PhysicalDevice& getPhysicalDevice() { return physicalDevice; };

        vk::Instance& getInstance() { return *instance; };

        [[nodiscard]] const QueueFamilies& getQueueFamilies() const { return queueFamilies; };

        const vk::AllocationCallbacks* getAllocationCallbacks() { return allocator; };

        vk::Queue& getPresentQueue() { return presentQueue; };
        vk::Queue& getTransferQueue() { return transferQueue; };
        vk::Queue& getGraphicsQueue() { return graphicsQueue; };
        vk::Queue& getComputeQueue() { return computeQueue; };

        vk::SurfaceKHR getSurface() { return surface; };

        /// Queries the format and present modes from a given physical device
        Carrot::SwapChainSupportDetails querySwapChainSupport(const vk::PhysicalDevice& device);

        /// Find the memory type compatible with the filter and the given properties
        uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);

        vk::CommandPool& getThreadComputeCommandPool() { return *computeCommandPool.get(); };
        vk::CommandPool& getThreadGraphicsCommandPool() { return *graphicsCommandPool.get(); };
        vk::CommandPool& getThreadTransferCommandPool() { return *transferCommandPool.get(); };

        template<typename CommandBufferConsumer>
        void performSingleTimeCommands(vk::CommandPool& commandPool, vk::Queue& queue, CommandBufferConsumer consumer);

        /// Performs a transfer operation on the transfer queue.
        /// \tparam CommandBufferConsumer function describing the operation. Takes a single vk::CommandBuffer& argument, and returns void.
        /// \param consumer function describing the operation
        template<typename CommandBufferConsumer>
        void performSingleTimeTransferCommands(CommandBufferConsumer consumer);

        /// Performs a graphics operation on the graphics queue.
        /// \tparam CommandBufferConsumer function describing the operation. Takes a single vk::CommandBuffer& argument, and returns void.
        /// \param consumer function describing the operation
        template<typename CommandBufferConsumer>
        void performSingleTimeGraphicsCommands(CommandBufferConsumer consumer);

        [[nodiscard]] vk::UniqueImageView createImageView(const vk::Image& image, vk::Format imageFormat,
                                                          vk::ImageAspectFlags aspectMask = vk::ImageAspectFlagBits::eColor,
                                                          vk::ImageViewType viewType = vk::ImageViewType::e2D, uint32_t layerCount = 1);

        std::set<uint32_t> createGraphicsAndTransferFamiliesSet();

        Render::Texture& getDefaultTexture();

        const vk::Sampler& getLinearSampler() { return *linearRepeatSampler; };
        const vk::Sampler& getNearestSampler() { return *nearestRepeatSampler; };

        /// Find the best available format for the depth texture
        vk::Format findDepthFormat();

        /// Find the best available format in the given candidates
        vk::Format findSupportedFormat(const vector<vk::Format>& candidates, vk::ImageTiling tiling,
                                       vk::FormatFeatureFlags features);

        const vk::Extent2D& getSwapchainExtent() const { return swapchainExtent; };

        vector<std::shared_ptr<Render::Texture>>& getSwapchainTextures() { return swapchainTextures; };

        size_t getSwapchainImageCount() const { return swapchainTextures.size(); };

        vk::Format getSwapchainImageFormat() const { return swapchainImageFormat; };

        /// Create the swapchain
        void createSwapChain();

        void cleanupSwapchain();

        vk::SwapchainKHR& getSwapchain() { return *swapchain; };

        void createUniformBuffers();

        vector<shared_ptr<Buffer>>& getDebugUniformBuffers() { return debugUniformBuffers; };
        vector<shared_ptr<Buffer>>& getCameraUniformBuffers() { return cameraUniformBuffers; };

        NakedPtr<GLFWwindow>& getWindow() { return window; };

        vk::Format getDepthFormat() { return depthFormat; };

        Render::TextureRepository& getTextureRepository() { return *textureRepository; };

        void updateViewportAndScissor(vk::CommandBuffer& commands);

        void fetchNewFramebufferSize();

        void onSwapchainImageCountChange(size_t newCount) override;

        void onSwapchainSizeChange(int newWidth, int newHeight) override;
    };
}

#include "VulkanDriver.ipp"