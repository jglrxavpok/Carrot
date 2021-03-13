//
// Created by jglrxavpok on 21/11/2020.
//
#pragma once
#include <map>
#include <memory>
#include <optional>
#include <vector>
#include <set>

namespace Carrot {
    class Engine;
}

#include <engine/vulkan/includes.h>
#include <GLFW/glfw3.h>
#include "engine/memory/NakedPtr.hpp"
#include "engine/render/IDTypes.h"
#include "engine/vulkan/CustomTracyVulkan.h"
#include "engine/render/DebugBufferObject.h"
#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#include "backends/imgui_impl_glfw.h"

#include "engine/vulkan/VulkanDriver.h"

using namespace std;

namespace Game {
    class Game;
}

namespace Carrot {
    class Buffer;

    class BufferView;

    class Image;

    class Mesh;

    class Model;

    class Pipeline;

    class Material;

    class InstanceData;

    class Camera;

    class GBuffer;

    class RayTracer;

    class ASBuilder;

    class ResourceAllocator;

    /// Base class interfacing with Vulkan
    class Engine {
    public:
        vector<unique_ptr<TracyVulkanContext>> tracyCtx{};

        /// Init the engine with the given GLFW window. Will immediately load Vulkan resources
        explicit Engine(NakedPtr<GLFWwindow> window);

        /// Launch the engine loop
        void run();

        /// Called by GLFW when the window is resized
        void onWindowResize();

        void onMouseMove(double xpos, double ypos);

        void onKeyEvent(int key, int scancode, int action, int mods);

        /// Cleanup resources
        ~Engine();

        /// Vulkan logical device
        vk::Device& getLogicalDevice();

        /// Queue families used by the engine
        const Carrot::QueueFamilies& getQueueFamilies();

        /// Vulkan Allocator
        vk::Optional<const vk::AllocationCallbacks> getAllocator();

        /// Command pool for transfer operations
        vk::CommandPool& getTransferCommandPool();

        /// Command pool for graphics operations
        vk::CommandPool& getGraphicsCommandPool();

        /// Command pool for compute operations
        vk::CommandPool& getComputeCommandPool();

        /// Queue for transfer operations
        vk::Queue getTransferQueue();

        /// Queue for graphics operations
        vk::Queue getGraphicsQueue();

        vk::Queue getPresentQueue();

        vk::Queue getComputeQueue();

        /// Create an image view from a given image
        [[nodiscard]] vk::UniqueImageView createImageView(const vk::Image& image, vk::Format imageFormat,
                                                          vk::ImageAspectFlags aspectMask = vk::ImageAspectFlagBits::eColor);

        // templates

        /// Performs a transfer operation on the transfer queue.
        /// \tparam CommandBufferConsumer function describing the operation. Takes a single vk::CommandBuffer& argument, and returns void.
        /// \param consumer function describing the operation
        template<typename CommandBufferConsumer>
        void performSingleTimeTransferCommands(CommandBufferConsumer&& consumer);

        /// Performs a graphics operation on the graphics queue.
        /// \tparam CommandBufferConsumer function describing the operation. Takes a single vk::CommandBuffer& argument, and returns void.
        /// \param consumer function describing the operation
        template<typename CommandBufferConsumer>
        void performSingleTimeGraphicsCommands(CommandBufferConsumer&& consumer);

        shared_ptr<Pipeline> getOrCreatePipeline(const string& name);

        unique_ptr<Carrot::Image>& Carrot::Engine::getOrCreateTexture(const string& textureName);

        vk::UniqueImageView& Carrot::Engine::getOrCreateTextureView(const string& textureName);

        uint32_t getSwapchainImageCount();

        vector<shared_ptr<Buffer>>& getCameraUniformBuffers();

        vector<shared_ptr<Buffer>>& getDebugUniformBuffers();

        const vk::UniqueSampler& getLinearSampler();

        vk::UniqueImageView& getDefaultImageView();

        shared_ptr<Material> getOrCreateMaterial(const string& name);

        /// Creates a set with the graphics and transfer family indices
        set<uint32_t> createGraphicsAndTransferFamiliesSet();

        Camera& getCamera();

        void updateViewportAndScissor(vk::CommandBuffer& commands);

        vk::Extent2D getSwapchainExtent() const;

        vk::Format getDepthFormat() const;

        vk::Format getSwapchainImageFormat() const;

        vk::PhysicalDevice& getPhysicalDevice();

        ASBuilder& getASBuilder();

        vector<vk::UniqueImageView>& getUIImageViews();

        bool grabbingCursor() const { return grabCursor; };

        RayTracer& getRayTracer() { return *raytracer; };

        ResourceAllocator& getResourceAllocator() { return *resourceAllocator; };

        VulkanDriver& getVulkanDriver() { return vkDriver; };

    private:
        double mouseX = 0.0;
        double mouseY = 0.0;
        float currentFPS = 0.0f;
        bool running = true;
        bool grabCursor = false;
        NakedPtr<GLFWwindow> window = nullptr;
        VulkanDriver vkDriver;
        int framebufferWidth;
        int framebufferHeight;
        uint32_t lastFrameIndex = 0;

        vk::UniqueSwapchainKHR swapchain{};
        vk::Format swapchainImageFormat = vk::Format::eUndefined;
        vk::Format depthFormat = vk::Format::eUndefined;
        vector<vk::Image> swapchainImages{}; // not unique because deleted with swapchain

        unique_ptr<ResourceAllocator> resourceAllocator;

        vector<unique_ptr<Image>> uiImages{};
        vector<vk::UniqueImageView> uiImageViews{};

        vk::Extent2D swapchainExtent{};
        vector<vk::UniqueImageView> swapchainImageViews{};

        vk::UniqueRenderPass gRenderPass{};
        vk::UniqueRenderPass imguiRenderPass{};
        map<string, shared_ptr<Pipeline>> pipelines{};
        vector<vk::UniqueFramebuffer> imguiFramebuffers{};
        vector<vk::UniqueFramebuffer> swapchainFramebuffers{};

        vk::UniqueCommandPool tracyCommandPool{};
        vector<vk::CommandBuffer> tracyCommandBuffers{};

        vector<vk::CommandBuffer> mainCommandBuffers{};
        vector<vk::CommandBuffer> gBufferCommandBuffers{};
        vector<vk::CommandBuffer> gResolveCommandBuffers{};
        vector<vk::CommandBuffer> uiCommandBuffers{};
        vector<vk::UniqueSemaphore> imageAvailableSemaphore{};
        vector<vk::UniqueSemaphore> renderFinishedSemaphore{};
        vector<vk::UniqueFence> inFlightFences{};
        vector<vk::UniqueFence> imagesInFlight{};

        // TODO: abstraction over textures
        map<string, unique_ptr<Image>> textureImages{};
        map<string, vk::UniqueImageView> textureImageViews{};
        map<string, shared_ptr<Material>> materials{};

        unique_ptr<Image> defaultImage = nullptr;
        vk::UniqueImageView defaultImageView{};

        vk::UniqueSampler linearRepeatSampler{};
        vk::UniqueSampler nearestRepeatSampler{};

        vector<shared_ptr<Buffer>> cameraUniformBuffers{};
        vector<shared_ptr<Buffer>> debugUniformBuffers{};
        vk::UniqueDescriptorPool imguiDescriptorPool{};
        vector<vk::DescriptorSet> descriptorSets{}; // not unique pointers because owned by descriptor pool

        unique_ptr<RayTracer> raytracer = nullptr;
        unique_ptr<ASBuilder> asBuilder = nullptr;
        unique_ptr<GBuffer> gBuffer = nullptr;

        unique_ptr<Camera> camera = nullptr;
        unique_ptr<Game::Game> game = nullptr;

        bool framebufferResized = false;

        /// Init engine
        void init();

        /// Init window
        void initWindow();

        /// Init Vulkan for rendering
        void initVulkan();

        void initImgui();

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

        void createUIResources();

        /// Create the render pass
        void createRenderPasses();

        /// Create the pipeline responsible for lighting via the gbuffer
        void createGBuffer();

        /// Create the object responsible of raytracing operations and subpasses
        void createRayTracer();

        /// Create the framebuffers to render to
        void createFramebuffers();

        /// Create the command pool for graphics operations
        void createGraphicsCommandPool();

        /// Create the command pool for compute operations
        void createComputeCommandPool();

        /// Create the primary command buffers for rendering
        void recordMainCommandBuffer(size_t frameIndex);

        void recordSecondaryCommandBuffers(size_t frameIndex);

        void recordGBufferPass(size_t frameIndex, vk::CommandBuffer& commandBuffer);

        /// Acquires a swapchain image, prepares UBOs, submit command buffer, and present to screen
        void drawFrame(size_t currentFrame);

        /// Update the game systems
        void tick(double deltaTime);

        void takeScreenshot();

        /// Create fences and semaphores used for rendering
        void createSynchronizationObjects();

        /// Recreate the swapchain when the window is resized
        void recreateSwapchain();

        /// Cleanup swapchain resources after window resizing
        void cleanupSwapchain();

        /// Create the UBOs for rendering
        void createUniformBuffers();

        /// Update the uniform buffer at index 'imageIndex'
        void updateUniformBuffer(int imageIndex);

        /// Find the best available format for the depth texture
        vk::Format findDepthFormat();

        /// Find the best available format in the given candidates
        vk::Format findSupportedFormat(const vector<vk::Format>& candidates, vk::ImageTiling tiling,
                                       vk::FormatFeatureFlags features);

        /// Create the samplers used by the engine
        void createSamplers();

        void createDefaultTexture();

        void initGame();

        void createCamera();

        void createTracyContexts();

        void allocateGraphicsCommandBuffers();

        vk::Instance& getVkInstance() { return vkDriver.getInstance(); };

    };
}

#include "Engine.ipp"