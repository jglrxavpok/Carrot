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

namespace Carrot {
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

    class VulkanDriver {
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

        vk::UniqueCommandPool graphicsCommandPool{};
        vk::UniqueCommandPool transferCommandPool{};
        vk::UniqueCommandPool computeCommandPool{};

        std::unique_ptr<Image> defaultImage = nullptr;
        vk::UniqueImageView defaultImageView{};

        vk::UniqueSampler linearRepeatSampler{};
        vk::UniqueSampler nearestRepeatSampler{};

        vk::UniqueSwapchainKHR swapchain{};
        vk::Format swapchainImageFormat = vk::Format::eUndefined;
        vk::Format depthFormat = vk::Format::eUndefined;
        vector<vk::Image> swapchainImages{}; // not unique because deleted with swapchain
        vk::Extent2D swapchainExtent{};
        vector<vk::UniqueImageView> swapchainImageViews{};

        vector<shared_ptr<Buffer>> cameraUniformBuffers{};
        vector<shared_ptr<Buffer>> debugUniformBuffers{};

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
        void createTransferCommandPool();

        /// Create the command pool for graphic operations
        void createGraphicsCommandPool();

        /// Create the command pool for compute operations
        void createComputeCommandPool();

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

        /// Create the image views used by the swapchain
        void createSwapChainImageViews();

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

        vk::CommandPool& getComputeCommandPool() { return *computeCommandPool; };
        vk::CommandPool& getGraphicsCommandPool() { return *graphicsCommandPool; };
        vk::CommandPool& getTransferCommandPool() { return *transferCommandPool; };

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
                                                          vk::ImageAspectFlags aspectMask = vk::ImageAspectFlagBits::eColor);

        std::set<uint32_t> createGraphicsAndTransferFamiliesSet();

        vk::UniqueImageView& getDefaultImageView();

        const vk::Sampler& getLinearSampler() { return *linearRepeatSampler; };

        /// Find the best available format for the depth texture
        vk::Format findDepthFormat();

        /// Find the best available format in the given candidates
        vk::Format findSupportedFormat(const vector<vk::Format>& candidates, vk::ImageTiling tiling,
                                       vk::FormatFeatureFlags features);

        const vk::Extent2D& getSwapchainExtent() const { return swapchainExtent; };

        vector<vk::Image>& getSwapchainImages() { return swapchainImages; };
        vector<vk::UniqueImageView>& getSwapchainImageViews() { return swapchainImageViews; };

        size_t getSwapchainImageCount() const { return swapchainImages.size(); };

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

        void updateViewportAndScissor(vk::CommandBuffer& commands);

        void recreateSwapchain();
    };
}

#include "VulkanDevice.ipp"