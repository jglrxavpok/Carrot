//
// Created by jglrxavpok on 21/11/2020.
//
#pragma once
#include <memory>
#include <optional>
#include <vector>
#include <set>

#include <vulkan/includes.h>
#include <GLFW/glfw3.h>
#include "memory/NakedPtr.hpp"

using namespace std;

namespace Carrot {
    class Buffer;
    class Image;

    struct QueueFamilies {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;
        std::optional<uint32_t> transferFamily;

        bool isComplete() const;
    };

    struct SwapChainSupportDetails {
        vk::SurfaceCapabilitiesKHR capabilities;
        std::vector<vk::SurfaceFormatKHR> formats;
        std::vector<vk::PresentModeKHR> presentModes;
    };

    class Engine {
    public:
        explicit Engine(NakedPtr<GLFWwindow> window);

        void run();

        void onWindowResize();

        /// Cleanup resources
        ~Engine();

        vk::Device& getLogicalDevice();
        Carrot::QueueFamilies& getQueueFamilies();

        vk::Optional<const vk::AllocationCallbacks> getAllocator();

        uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);

        vk::CommandPool& getTransferCommandPool();
        vk::CommandPool& getGraphicsCommandPool();

        vk::Queue getTransferQueue();

        vk::Queue getGraphicsQueue();

        [[nodiscard]] vk::UniqueImageView createImageView(const vk::Image& image, vk::Format imageFormat, vk::ImageAspectFlags aspectMask = vk::ImageAspectFlagBits::eColor) const;

        // templates

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

        /// Performs an operation on the given queue.
        /// \tparam CommandBufferConsumer function describing the operation. Takes a single vk::CommandBuffer& argument, and returns void.
        /// \param consumer function describing the operation
        template<typename CommandBufferConsumer>
        void performSingleTimeCommands(vk::CommandPool& commandPool, vk::Queue& queue, CommandBufferConsumer consumer);

    private:
        bool running = true;
        NakedPtr<GLFWwindow> window = nullptr;
        const vk::AllocationCallbacks* allocator = nullptr;

        vk::UniqueInstance instance;
        vk::UniqueDebugUtilsMessengerEXT callback{};
        vk::PhysicalDevice physicalDevice{};
        vk::UniqueDevice device{};
        QueueFamilies queueFamilies{};
        vk::Queue graphicsQueue{};
        vk::Queue presentQueue{};
        vk::Queue transferQueue{};
        vk::SurfaceKHR surface{};
        vk::UniqueSwapchainKHR swapchain{};
        vk::Format swapchainImageFormat = vk::Format::eUndefined;
        vk::Format depthFormat = vk::Format::eUndefined;
        vector<vk::Image> swapchainImages{}; // not unique because deleted with swapchain
        unique_ptr<Image> depthImage{};
        vk::Extent2D swapchainExtent{};
        vector<vk::UniqueImageView> swapchainImageViews{};
        vk::UniqueImageView depthImageView{};
        vk::UniqueRenderPass renderPass{};
        vk::UniquePipelineLayout pipelineLayout{};
        vk::UniquePipeline graphicsPipeline{};
        vector<vk::UniqueFramebuffer> swapchainFramebuffers{};
        vk::UniqueCommandPool graphicsCommandPool{};
        vk::UniqueCommandPool transferCommandPool{};
        vector<vk::CommandBuffer> commandBuffers{};
        vector<vk::UniqueSemaphore> imageAvailableSemaphore{};
        vector<vk::UniqueSemaphore> renderFinishedSemaphore{};
        vector<vk::UniqueFence> inFlightFences{};
        vector<vk::UniqueFence> imagesInFlight{};

        unique_ptr<Buffer> vertexBuffer = nullptr;
        unique_ptr<Buffer> indexBuffer = nullptr;

        // TODO: abstraction over textures
        unique_ptr<Image> texture = nullptr;
        vk::UniqueImageView textureView{};
        vk::UniqueSampler linearRepeatSampler{};
        vk::UniqueSampler nearestRepeatSampler{};

        vk::UniqueDescriptorSetLayout descriptorSetLayout{};
        vector<shared_ptr<Buffer>> uniformBuffers{};
        vk::UniqueDescriptorPool descriptorPool{};
        vector<vk::DescriptorSet> descriptorSets{}; // not unique pointers because owned by descriptor pool

        bool framebufferResized = false;

        /// Init engine
        void init();

        /// Init window
        void initWindow();

        /// Init Vulkan for rendering
        void initVulkan();

        /// Create Vulkan instance
        void createInstance();

        /// Check validation layers are supported (if NO_DEBUG disabled)
        bool checkValidationLayerSupport();

        /// Generate a vector with the required extensions for this application
        vector<const char *> getRequiredExtensions();

        /// Setups debug messages
        void setupDebugMessenger();

        /// Prepares a debug messenger
        void setupMessenger(vk::DebugUtilsMessengerCreateInfoEXT &ext);

        /// Select a GPU
        void pickPhysicalDevice();

        /// Rank physical device based on their capabilities
        int ratePhysicalDevice(const vk::PhysicalDevice& device);

        /// Gets the queue indices of a given physical device
        Carrot::QueueFamilies findQueueFamilies(const vk::PhysicalDevice& device);

        /// Create the logical device to interface with Vulkan
        void createLogicalDevice();

        /// Create the rendering surface for Vulkan
        void createSurface();

        /// Check the given device supports the extensions inside VULKAN_DEVICE_EXTENSIONS (constants.h)
        bool checkDeviceExtensionSupport(const vk::PhysicalDevice &logicalDevice);

        /// Queries the format and present modes from a given physical device
        Carrot::SwapChainSupportDetails querySwapChainSupport(const vk::PhysicalDevice& device);

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

        void createSwapChain();

        void createSwapChainImageViews();

        void createGraphicsPipeline();

        vk::UniqueShaderModule createShaderModule(const vector<char>& bytecode);

        void createRenderPass();

        void createFramebuffers();

        void createGraphicsCommandPool();

        void createCommandBuffers();

        void drawFrame(size_t currentFrame);

        void createSynchronizationObjects();

        void recreateSwapchain();

        void cleanupSwapchain();

        void createVertexBuffer();

        void createTransferCommandPool();

        void createIndexBuffer();

        void createDescriptorSetLayout();

        void createUniformBuffers();

        set<uint32_t> createGraphicsAndTransferFamiliesSet();

        void updateUniformBuffer(int imageIndex);

        void createDescriptorSets();

        void createDescriptorPool();

        void createDepthTexture();

        vk::Format findDepthFormat();

        vk::Format findSupportedFormat(const vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features);

        void createTexture();

        // Templates

        template<typename T>
        vk::UniqueDeviceMemory allocate(vk::Device& device, vk::Buffer& buffer, const vector<T>& data);

        template<typename T>
        void uploadBuffer(vk::Device& device, vk::Buffer& buffer, vk::DeviceMemory& memory, const vector<T>& data);

        void createSamplers();
    };
}

// template implementation
#include "Engine.ipp"
