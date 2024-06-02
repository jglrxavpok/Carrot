//
// Created by jglrxavpok on 11/03/2021.
//

#pragma once

#include "includes.h"
#include <functional>
#include <optional>
#include <utility>
#include <vector>
#include <set>
#include <core/memory/NakedPtr.hpp>
#include <GLFW/glfw3.h>
#include "engine/vulkan/SwapchainAware.h"
#include "engine/vulkan/SynchronizedQueue.h"
#include "core/memory/ThreadLocal.hpp"
#include "engine/Configuration.h"
#include "engine/Window.h"
#include "engine/render/resources/DeviceMemory.h"

namespace sol {
    class state;
}

namespace Carrot {
    namespace Render {
        class Texture;
        class TextureRepository;
        struct Context;
        enum class Eye;
    };

    namespace VR {
        class Interface;
        class Session;
    }

    class Image;
    class Buffer;
    class Engine;
    class VulkanRenderer;

    struct QueueFamilies {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;
        std::optional<uint32_t> transferFamily;
        std::optional<uint32_t> computeFamily;

        bool isComplete() const;
    };

    template<typename TResource>
    class DeferredDestruction {
    public:
        explicit DeferredDestruction(std::string name, TResource&& toDestroy, std::size_t framesBeforeDestruction):
            countdown(framesBeforeDestruction),
            resourceName(std::move(name))
            {
            resource = std::move(toDestroy);
        };
        explicit DeferredDestruction(const std::string& name, TResource&& toDestroy): DeferredDestruction(name, std::move(toDestroy), MAX_FRAMES_IN_FLIGHT) {};

        bool isReadyForDestruction() const {
            return countdown == 0;
        }

        void tickDown() {
            if(countdown > 0)
                countdown--;
        }

    private:
        std::string resourceName; // destroyed AFTER the resource, helps debug use-after-free
    public:
        TResource resource{};
    private:
        std::size_t countdown = 0; /* number of frames before destruction (swapchain image count by default)*/
    };

    using DeferredImageDestruction = DeferredDestruction<vk::UniqueImage>;
    using DeferredImageViewDestruction = DeferredDestruction<vk::UniqueImageView>;
    using DeferredMemoryDestruction = DeferredDestruction<Carrot::DeviceMemory>;
    using DeferredBufferDestruction = DeferredDestruction<vk::UniqueBuffer>;
    using DeferredAccelerationStructureDestruction = DeferredDestruction<vk::UniqueAccelerationStructureKHR>;

    class VulkanDriver: public SwapchainAware {
    public:
        VulkanDriver(Carrot::Window& window, Configuration config, Engine* engine, Carrot::VR::Interface* vrInterface);

        ~VulkanDriver();

        /// Gets the queue indices of a given physical device
        QueueFamilies findQueueFamilies(const vk::PhysicalDevice& device);

        vk::Device& getLogicalDevice() { return *device; };

        vk::PhysicalDevice& getPhysicalDevice() { return physicalDevice; };

        vk::Instance& getInstance() { return *instance; };

        [[nodiscard]] const QueueFamilies& getQueueFamilies() const { return queueFamilies; };

        const vk::AllocationCallbacks* getAllocationCallbacks() { return allocator; };

        Vulkan::SynchronizedQueue& getPresentQueue() { return presentQueue; };
        Vulkan::SynchronizedQueue& getTransferQueue() { return transferQueue; };
        Vulkan::SynchronizedQueue& getGraphicsQueue() { return graphicsQueue; };
        std::uint32_t getGraphicsQueueIndex() { return graphicsQueueIndex; };
        Vulkan::SynchronizedQueue& getComputeQueue() { return computeQueue; };

        /// Queries the format and present modes from a given physical device
        Carrot::SwapChainSupportDetails querySwapChainSupport(const vk::PhysicalDevice& device);

        /// Find the memory type compatible with the filter and the given properties
        uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);

        vk::CommandPool& getThreadComputeCommandPool() { return *computeCommandPool.get(); };
        vk::CommandPool& getThreadGraphicsCommandPool() { return *graphicsCommandPool.get(); };
        vk::CommandPool& getThreadTransferCommandPool() { return *transferCommandPool.get(); };

        /// Performs a transfer operation on the transfer queue.
        /// \tparam CommandBufferConsumer function describing the operation. Takes a single vk::CommandBuffer& argument, and returns void.
        /// \param consumer function describing the operation
        template<typename CommandBufferConsumer>
        void performSingleTimeTransferCommands(CommandBufferConsumer consumer, bool waitFor = true, vk::Semaphore waitSemaphore = {}, vk::PipelineStageFlags2 waitDstFlags = static_cast<vk::PipelineStageFlagBits2>(0), vk::Semaphore signalSemaphore = {});

        /// Performs a graphics operation on the graphics queue.
        /// \tparam CommandBufferConsumer function describing the operation. Takes a single vk::CommandBuffer& argument, and returns void.
        /// \param consumer function describing the operation
        template<typename CommandBufferConsumer>
        void performSingleTimeGraphicsCommands(CommandBufferConsumer consumer, bool waitFor = true, vk::Semaphore waitSemaphore = {}, vk::PipelineStageFlags2 waitDstFlags = static_cast<vk::PipelineStageFlagBits2>(0), vk::Semaphore signalSemaphore = {});

        [[nodiscard]] vk::UniqueImageView createImageView(const vk::Image& image, vk::Format imageFormat,
                                                          vk::ImageAspectFlags aspectMask = vk::ImageAspectFlagBits::eColor,
                                                          vk::ImageViewType viewType = vk::ImageViewType::e2D, uint32_t layerCount = 1);

        std::set<uint32_t> createGraphicsAndTransferFamiliesSet();

        Render::Texture& getDefaultTexture();

        const vk::Sampler& getLinearSampler() const;
        const vk::Sampler& getNearestSampler() const;

        /// Find the best available format for the depth texture
        vk::Format findDepthFormat();

        /// Find the best available format in the given candidates
        vk::Format findSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling,
                                       vk::FormatFeatureFlags features);

        const vk::Extent2D& getFinalRenderSize(Window& window) const;

        size_t getSwapchainImageCount() const;

        vk::Format getSwapchainImageFormat() const;

        void createUniformBuffers();

        Window& getMainWindow();

        vk::Format getDepthFormat() { return depthFormat; };

        Render::TextureRepository& getTextureRepository() { return *textureRepository; };

        const Configuration& getConfiguration() { return config; }

        bool hasDebugNames() const;

        vk::DescriptorSetLayout& getEmptyDescriptorSetLayout() { return *emptyDescriptorSetLayout; }

        Engine& getEngine();
        VulkanRenderer& getRenderer();

        void newFrame(const Carrot::Render::Context& renderContext);

    public: // debug
        void breakOnNextVulkanError();

        //! Called when a device lost occurs, used to wait for Aftermath to complete its work
        void onDeviceLost();

        //! Adds a marker to the given command buffer, to help debugging if the GPU crashes
        void setMarker(vk::CommandBuffer& cmds, const std::string& markerData);

        template<typename... Args>
        void setFormattedMarker(vk::CommandBuffer& cmds, const char* format, Args&&... args) {
            setMarker(cmds, Carrot::sprintf(format, std::forward<Args>(args)...));
        }

        /// Writes current active gpu resources to "./gpu-resources.txt"
        void dumpActiveGPUResources();

    public:
        const vk::PhysicalDeviceFeatures& getPhysicalDeviceFeatures() const;
        const vk::PhysicalDeviceLimits& getPhysicalDeviceLimits() const;

    public:
        void deferDestroy(const std::string& name, vk::UniqueImage&& image);
        void deferDestroy(const std::string& name, vk::UniqueImageView&& imageView);
        void deferDestroy(const std::string& name, Carrot::DeviceMemory&& memory);
        void deferDestroy(const std::string& name, vk::UniqueBuffer&& resource);
        void deferDestroy(const std::string& name, vk::UniqueAccelerationStructureKHR&& resource);

    public: // swapchain & viewport
        void updateViewportAndScissor(vk::CommandBuffer& commands, const vk::Extent2D& size);

        void onSwapchainImageCountChange(size_t newCount) override;

        void onSwapchainSizeChange(Window& window, int newWidth, int newHeight) override;

    public:
        void deferCommandBufferDestruction(vk::CommandPool commandPool, vk::CommandBuffer commandBuffer);

    public:
        static void registerUsertype(sol::state& destination);

    private:
        struct DeferredCommandBufferDestruction {
            vk::CommandPool pool;
            vk::CommandBuffer buffer;

            explicit DeferredCommandBufferDestruction(vk::CommandPool pool, vk::CommandBuffer buffer): pool(pool), buffer(buffer) {}
        };

        Window& mainWindow;
        Configuration config;

        std::mutex deviceMutex;

        const vk::AllocationCallbacks* allocator = nullptr;
        Engine* engine = nullptr;

        VR::Interface* vrInterface = nullptr;

        vk::UniqueInstance instance;
        vk::UniqueDebugUtilsMessengerEXT callback{};
        vk::PhysicalDevice physicalDevice{};
        vk::UniqueDevice device{};
        QueueFamilies queueFamilies{};

        std::uint32_t graphicsQueueIndex = -1;
        Vulkan::SynchronizedQueue graphicsQueue;
        Vulkan::SynchronizedQueue presentQueue;
        Vulkan::SynchronizedQueue transferQueue;
        Vulkan::SynchronizedQueue computeQueue;

        std::list<DeferredImageDestruction> deferredImageDestructions;
        std::list<DeferredImageViewDestruction> deferredImageViewDestructions;
        std::list<DeferredMemoryDestruction> deferredMemoryDestructions;
        std::list<DeferredBufferDestruction> deferredBufferDestructions;
        std::list<DeferredAccelerationStructureDestruction> deferredAccelerationStructureDestructions;

        ThreadLocal<vk::UniqueCommandPool> graphicsCommandPool;
        ThreadLocal<vk::UniqueCommandPool> transferCommandPool;
        ThreadLocal<vk::UniqueCommandPool> computeCommandPool;

        std::shared_ptr<Render::Texture> defaultTexture = nullptr;

        vk::UniqueSampler linearRepeatSampler{};
        vk::UniqueSampler nearestRepeatSampler{};

        vk::PhysicalDeviceLimits physicalDeviceLimits{};
        vk::PhysicalDeviceFeatures physicalDeviceFeatures{};
        vk::Format depthFormat = vk::Format::eUndefined;

        vk::UniqueDescriptorSetLayout emptyDescriptorSetLayout{};

        std::unique_ptr<Render::TextureRepository> textureRepository = nullptr;

        Async::SpinLock deferredDestroysLock;
        std::unordered_map<std::uint32_t, std::vector<DeferredCommandBufferDestruction>> deferredCommandBufferDestructions;

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

        /// Fills the engine's capabilities based on the current graphics driver & hardware
        void fillRenderingCapabilities();

        /// Create the logical device to interface with Vulkan
        void createLogicalDevice();

        /// Check the given device supports the extensions inside VULKAN_DEVICE_EXTENSIONS (constants.h)
        bool checkDeviceExtensionSupport(const vk::PhysicalDevice& logicalDevice);

        /// Create the command pool for transfer operations
        vk::UniqueCommandPool createTransferCommandPool();

        /// Create the command pool for graphic operations
        vk::UniqueCommandPool createGraphicsCommandPool();

        /// Create the command pool for compute operations
        vk::UniqueCommandPool createComputeCommandPool();

        void createDefaultTexture();

    public:
        void startFrame(const Render::Context& renderContext);

    public:
        void submitGraphics(const vk::SubmitInfo2& info, const vk::Fence& fence = {});
        void submitTransfer(const vk::SubmitInfo2& info, const vk::Fence& fence = {});
        void submitCompute(const vk::SubmitInfo2& info, const vk::Fence& fence = {});

        std::mutex& getDeviceMutex();

        /// Synchronized call to waitIdle of the logical device
        void waitDeviceIdle();

    private:
        template<typename CommandBufferConsumer>
        void performSingleTimeCommands(vk::CommandPool& commandPool, const std::function<void(const vk::SubmitInfo2&, const vk::Fence&)>& submitAction, const std::function<void()>& waitAction, bool waitFor, vk::Semaphore waitSemaphore, vk::PipelineStageFlags2 waitDstFlags, vk::Semaphore signalSemaphore, CommandBufferConsumer consumer);

        // used by performSingleTimeCommands
        static void waitGraphics();
        static void waitTransfer();

    private: // memory inspection support
        vk::PhysicalDeviceMemoryProperties baseProperties{};

        bool memoryBudgetSupported = false;
        vk::DeviceSize gpuHeapBudgets[VK_MAX_MEMORY_HEAPS] = {0};
        vk::DeviceSize gpuHeapUsages[VK_MAX_MEMORY_HEAPS] = {0};
    };
}

#include "VulkanDriver.ipp"