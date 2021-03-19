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
#include "engine/render/VulkanRenderer.h"

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

        uint32_t getSwapchainImageCount();

        vector<shared_ptr<Buffer>>& getCameraUniformBuffers();

        vector<shared_ptr<Buffer>>& getDebugUniformBuffers();

        shared_ptr<Material> getOrCreateMaterial(const string& name);

        /// Creates a set with the graphics and transfer family indices
        set<uint32_t> createGraphicsAndTransferFamiliesSet();

        Camera& getCamera();

        vk::PhysicalDevice& getPhysicalDevice();

        ASBuilder& getASBuilder();

        bool grabbingCursor() const { return grabCursor; };

        RayTracer& getRayTracer() { return renderer.getRayTracer(); };

        ResourceAllocator& getResourceAllocator() { return *resourceAllocator; };

        VulkanDriver& getVulkanDriver() { return vkDriver; };

        VulkanRenderer& getRenderer() { return renderer; };

        GBuffer& getGBuffer() { return renderer.getGBuffer(); };

    private:
        double mouseX = 0.0;
        double mouseY = 0.0;
        float currentFPS = 0.0f;
        bool running = true;
        bool grabCursor = false;
        NakedPtr<GLFWwindow> window = nullptr;
        VulkanDriver vkDriver;
        VulkanRenderer renderer;
        uint32_t lastFrameIndex = 0;

        unique_ptr<ResourceAllocator> resourceAllocator;

        vk::UniqueCommandPool tracyCommandPool{};
        vector<vk::CommandBuffer> tracyCommandBuffers{};

        vector<vk::CommandBuffer> mainCommandBuffers{};
        vector<vk::CommandBuffer> gBufferCommandBuffers{};
        vector<vk::CommandBuffer> gResolveCommandBuffers{};
        vector<vk::UniqueSemaphore> imageAvailableSemaphore{};
        vector<vk::UniqueSemaphore> renderFinishedSemaphore{};
        vector<vk::UniqueFence> inFlightFences{};
        vector<vk::UniqueFence> imagesInFlight{};

        map<string, shared_ptr<Material>> materials{};

        vector<vk::DescriptorSet> descriptorSets{}; // not unique pointers because owned by descriptor pool

        unique_ptr<Camera> camera = nullptr;
        unique_ptr<Game::Game> game = nullptr;

        bool framebufferResized = false;

        /// Init engine
        void init();

        /// Init window
        void initWindow();

        /// Init Vulkan for rendering
        void initVulkan();

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

        /// Update the uniform buffer at index 'imageIndex'
        void updateUniformBuffer(int imageIndex);

        void initGame();

        void createCamera();

        void createTracyContexts();

        void allocateGraphicsCommandBuffers();

    };
}

#include "Engine.ipp"