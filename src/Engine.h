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
    class Mesh;
    class Model;

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

    /// Base class interfacing with Vulkan
    class Engine {
    public:
        /// Init the engine with the given GLFW window. Will immediately load Vulkan resources
        explicit Engine(NakedPtr<GLFWwindow> window);

        /// Launch the engine loop
        void run();

        /// Called by GLFW when the window is resized
        void onWindowResize();

        /// Cleanup resources
        ~Engine();

        /// Vulkan logical device
        vk::Device& getLogicalDevice();

        /// Queue families used by the engine
        Carrot::QueueFamilies& getQueueFamilies();

        /// Vulkan Allocator
        vk::Optional<const vk::AllocationCallbacks> getAllocator();

        /// Find the memory type compatible with the filter and the given properties
        uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);

        /// Command pool for transfer operations
        vk::CommandPool& getTransferCommandPool();

        /// Command pool for graphics operations
        vk::CommandPool& getGraphicsCommandPool();

        /// Queue for transfer operations
        vk::Queue getTransferQueue();

        /// Queue for graphics operations
        vk::Queue getGraphicsQueue();

        /// Create an image view from a given image
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

        unique_ptr<Model> model = nullptr;
        unique_ptr<Mesh> testMesh = nullptr;

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

        /// Create the swapchain
        void createSwapChain();

        /// Create the image views used by the swapchain
        void createSwapChainImageViews();

        /// Create the graphics pipeline, using the shaders inside resources/shaders/
        void createGraphicsPipeline();

        /// Create a single shader module from the given bytecode
        vk::UniqueShaderModule createShaderModule(const vector<uint8_t>& bytecode);

        /// Create the render pass
        void createRenderPass();

        /// Create the framebuffers to render to
        void createFramebuffers();

        /// Create the command pool for graphics operations
        void createGraphicsCommandPool();

        /// Create the primary command buffers for rendering
        void createCommandBuffers();

        /// Acquires a swapchain image, prepares UBOs, submit command buffer, and present to screen
        void drawFrame(size_t currentFrame);

        /// Create fences and semaphores used for rendering
        void createSynchronizationObjects();

        /// Recreate the swapchain when the window is resized
        void recreateSwapchain();

        /// Cleanup swapchain resources after window resizing
        void cleanupSwapchain();

        /// Create the command pool for transfer operations
        void createTransferCommandPool();

        /// Create the descriptor set layout used by the pipeline
        void createDescriptorSetLayout();

        /// Create the UBOs for rendering
        void createUniformBuffers();

        /// Creates a set with the graphics and transfer family indices
        set<uint32_t> createGraphicsAndTransferFamiliesSet();

        /// Update the uniform buffer at index 'imageIndex'
        void updateUniformBuffer(int imageIndex);

        /// Create and fill the descriptor sets used by the pipeline
        void createDescriptorSets();

        /// Create the descriptor pool
        void createDescriptorPool();

        /// Create the depth texture, used for depth testing
        void createDepthTexture();

        /// Find the best available format for the depth texture
        vk::Format findDepthFormat();

        /// Find the best available format in the given candidates
        vk::Format findSupportedFormat(const vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features);

        /// Creates the albedo texture
        void createTexture();

        /// Create the samplers used by the engine
        void createSamplers();

        void createModel();

        void updateViewportAndScissor(vk::CommandBuffer& commands);
    };
}

// template implementation
#include "Engine.ipp"
