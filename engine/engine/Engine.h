//
// Created by jglrxavpok on 21/11/2020.
//
#pragma once
#include <map>
#include <memory>
#include <optional>
#include <vector>
#include <set>
#include <future>

namespace Carrot {
    class Engine;
}

#include <engine/Window.h>
#include <engine/vulkan/includes.h>
#include <engine/vulkan/SwapchainAware.h>
#include <GLFW/glfw3.h>
#include "core/memory/NakedPtr.hpp"
#include "engine/render/IDTypes.h"
#include "engine/vulkan/CustomTracyVulkan.h"
#include "engine/render/DebugBufferObject.h"
#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#include "backends/imgui_impl_glfw.h"

#include "engine/vulkan/VulkanDriver.h"
#include "engine/render/VulkanRenderer.h"
#include "render/Skybox.hpp"
#include "render/Composer.h"
#include "engine/Configuration.h"
#include "engine/io/actions/InputVectors.h"
#include "core/async/Coroutines.hpp"

namespace sol {
    class state;
}

namespace Carrot {
    class CarrotGame;

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

    namespace Render {
        class Texture;
        class Graph;

        namespace PassData {
            struct Composer;
            struct GResolve;
        }
    };

#ifdef ENABLE_VR
    namespace VR {
        class Interface;
        class Session;
    }
#endif

    /// Base class interfacing with Vulkan
    class Engine: public SwapchainAware {
    public:
        using FrameTask = std::function<void()>;

        std::vector<std::unique_ptr<TracyVulkanContext>> tracyCtx{};

        static Carrot::Engine& getInstance() { return *instance; }

        /// Starts the engine. Will immediately load Vulkan resources
        explicit Engine(Configuration config = {});

        /// Launch the engine loop
        void run();

        /// Stops the game, triggering shutdown process and closing window.
        void stop();

        std::unique_ptr<Carrot::CarrotGame>& getGame();


    public: // GLFW event handling

        /// Called by GLFW when the window is resized
        void onWindowResize();

        void onMouseMove(double xpos, double ypos, bool updateOnlyDelta);

        void onKeyEvent(int key, int scancode, int action, int mods);

        void onMouseButton(int button, int action, int mods);

    public:

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
        vk::Queue& getTransferQueue();

        /// Queue for graphics operations
        vk::Queue& getGraphicsQueue();

        vk::Queue& getPresentQueue();

        vk::Queue& getComputeQueue();

        // templates

        /// Performs a transfer operation on the transfer queue.
        /// \tparam CommandBufferConsumer function describing the operation. Takes a single vk::CommandBuffer& argument, and returns void.
        /// \param consumer function describing the operation
        template<typename CommandBufferConsumer>
        void performSingleTimeTransferCommands(CommandBufferConsumer&& consumer, bool waitFor = true, vk::Semaphore waitSemaphore = {}, vk::PipelineStageFlags waitDstFlags = static_cast<vk::PipelineStageFlagBits>(0));

        /// Performs a graphics operation on the graphics queue.
        /// \tparam CommandBufferConsumer function describing the operation. Takes a single vk::CommandBuffer& argument, and returns void.
        /// \param consumer function describing the operation
        template<typename CommandBufferConsumer>
        void performSingleTimeGraphicsCommands(CommandBufferConsumer&& consumer, bool waitFor = true, vk::Semaphore waitSemaphore = {}, vk::PipelineStageFlags waitDstFlags = static_cast<vk::PipelineStageFlagBits>(0));

        std::uint32_t getSwapchainImageCount();

        std::vector<std::shared_ptr<Buffer>>& getDebugUniformBuffers();

        /// Creates a set with the graphics and transfer family indices
        std::set<uint32_t> createGraphicsAndTransferFamiliesSet();

        Camera& getMainViewportCamera(Carrot::Render::Eye eye);
        Camera& getCamera();

        vk::PhysicalDevice& getPhysicalDevice();

        ASBuilder& getASBuilder();

        bool isGrabbingCursor() const { return grabbingCursor; };

        RayTracer& getRayTracer() { return renderer.getRayTracer(); };

        ResourceAllocator& getResourceAllocator() { return *resourceAllocator; };

        VulkanDriver& getVulkanDriver() { return vkDriver; };

        VulkanRenderer& getRenderer() { return renderer; };

        GBuffer& getGBuffer() { return renderer.getGBuffer(); };

        void setSkybox(Skybox::Type type);

        void onSwapchainSizeChange(int newWidth, int newHeight) override;

        void onSwapchainImageCountChange(size_t newCount) override;

        void toggleCursorGrab() {
            if(grabbingCursor) {
                ungrabCursor();
            } else {
                grabCursor();
            }
        }

        void grabCursor() {
            grabbingCursor = true;
            glfwSetInputMode(window.getGLFWPointer(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }

        void ungrabCursor() {
            grabbingCursor = false;
            glfwSetInputMode(window.getGLFWPointer(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }

        bool hasPreviousFrame() const {
            return frames > 0;
        }

        uint32_t getPreviousFrameIndex() const {
            return lastFrameIndex;
        }

        Render::Composer& getMainComposer(Render::Eye eye = Render::Eye::NoVR) { return *composers[eye]; }

        Render::Context newRenderContext(std::size_t swapchainFrameIndex, Render::Viewport& viewport, Render::Eye eye = Render::Eye::NoVR);

        std::uint32_t getSwapchainImageIndexRightNow() { return swapchainImageIndexRightNow; }
#ifdef ENABLE_VR
        VR::Session& getVRSession() { return *vrSession; }
#endif

    public:
        Render::Pass<Render::PassData::GResolve>& fillInDefaultPipeline(Render::GraphBuilder& graphBuilder, Carrot::Render::Eye eye, std::function<void(const Carrot::Render::CompiledPass& pass, const Render::Context&, vk::CommandBuffer&)> opaqueCallback, std::function<void(const Carrot::Render::CompiledPass& pass, const Render::Context&, vk::CommandBuffer&)> transparentCallback);

    public: // viewports
        Render::Viewport& getMainViewport();
        Render::Viewport& createViewport();

    public: // inputs

        using KeyCallback = std::function<void(int key, int scancode, int action, int mods)>;
        using GamepadButtonCallback = std::function<void(int joystickID, int button, bool isPressed)>;
        using GamepadAxisCallback = std::function<void(int joystickID, int axisID, float newValue, float oldValue)>;
        using GamepadVec2Callback = std::function<void(int joystickID, IO::GameInputVectorType vecID, glm::vec2 newValue, glm::vec2 oldValue)>;
        using KeysVec2Callback = std::function<void(IO::GameInputVectorType vecID, glm::vec2 newValue, glm::vec2 oldValue)>;
        using MouseButtonCallback = std::function<void(int button, bool isPressed, int mods)>;
        using MousePositionCallback = std::function<void(double xpos, double ypos)>;
        using MouseDeltaCallback = std::function<void(double dx, double dy)>;

        Carrot::UUID addGLFWKeyCallback(KeyCallback keyCallback) {
            Carrot::UUID uuid;
            keyCallbacks[uuid] = keyCallback;
            return uuid;
        }

        Carrot::UUID addGLFWMouseButtonCallback(MouseButtonCallback callback) {
            Carrot::UUID uuid;
            mouseButtonCallbacks[uuid] = callback;
            return uuid;
        }

        Carrot::UUID addGLFWGamepadButtonCallback(GamepadButtonCallback callback) {
            Carrot::UUID uuid;
            gamepadButtonCallbacks[uuid] = callback;
            return uuid;
        }

        Carrot::UUID addGLFWGamepadAxisCallback(GamepadAxisCallback callback) {
            Carrot::UUID uuid;
            gamepadAxisCallbacks[uuid] = callback;
            return uuid;
        }

        Carrot::UUID addGLFWGamepadVec2Callback(GamepadVec2Callback callback) {
            Carrot::UUID uuid;
            gamepadVec2Callbacks[uuid] = callback;
            return uuid;
        }

        Carrot::UUID addGLFWKeysVec2Callback(KeysVec2Callback callback) {
            Carrot::UUID uuid;
            keysVec2Callbacks[uuid] = callback;
            return uuid;
        }

        Carrot::UUID addGLFWMousePositionCallback(MousePositionCallback callback) {
            Carrot::UUID uuid;
            mousePositionCallbacks[uuid] = callback;
            return uuid;
        }

        Carrot::UUID addGLFWMouseDeltaCallback(MouseDeltaCallback callback) {
            Carrot::UUID uuid;
            mouseDeltaCallbacks[uuid] = callback;
            return uuid;
        }

        Carrot::UUID addGLFWMouseDeltaGrabbedCallback(MouseDeltaCallback callback) {
            Carrot::UUID uuid;
            mouseDeltaGrabbedCallbacks[uuid] = callback;
            return uuid;
        }

        void removeGLFWKeyCallback(const Carrot::UUID& uuid) {
            keyCallbacks.erase(uuid);
        }

        void removeGLFWMouseButtonCallback(const Carrot::UUID& uuid) {
            mouseButtonCallbacks.erase(uuid);
        }

        void removeGLFWGamepadButtonCallback(const Carrot::UUID& uuid) {
            gamepadButtonCallbacks.erase(uuid);
        }

        void removeGLFWGamepadAxisCallback(const Carrot::UUID& uuid) {
            gamepadAxisCallbacks.erase(uuid);
        }

        void removeGLFWGamepadVec2Callback(const Carrot::UUID& uuid) {
            gamepadVec2Callbacks.erase(uuid);
        }

        void removeGLFWKeysVec2Callback(const Carrot::UUID& uuid) {
            keysVec2Callbacks.erase(uuid);
        }

        void removeGLFWMousePositionCallback(const Carrot::UUID& uuid) {
            mousePositionCallbacks.erase(uuid);
        }

        void removeGLFWMouseDeltaCallback(const Carrot::UUID& uuid) {
            mouseDeltaCallbacks.erase(uuid);
        }

        void removeGLFWMouseDeltaGrabbedCallback(const Carrot::UUID& uuid) {
            mouseDeltaGrabbedCallbacks.erase(uuid);
        }

    public:
        const Configuration& getConfiguration() const { return config; }

    public: // async stuff
        /// co_awaits the next engine frame. Used for coroutines.
        Async::Task<> cowaitNextFrame();

        void transferOwnership(Async::Task<>&& task) {
            nextFrameAwaiter.transferOwnership(std::move(task));
        }

        /// Adds a task that will run parallel to other systems during the frame.
        /// WARNING: WILL BE WAITED AT THE END OF THE FRAME. DON'T DO ANYTHING THAT COULD FREEZE THE MAIN LOOP
        void addFrameTask(FrameTask&& task);

    private: // async private
        void waitForFrameTasks();

    public:
        static void registerUsertype(sol::state& destination);

    private:
        /// allows to set the 'instance' static variable during construction. Big hack, but lets systems access the engine during their construction
        struct SetterHack {
            explicit SetterHack(Carrot::Engine* e) {
                Carrot::Engine::instance = e;
            }
        } instanceSetterHack;

        Configuration config;
        Window window;
        double mouseX = 0.0;
        double mouseY = 0.0;
        float currentFPS = 0.0f;
        bool running = true;
        bool grabbingCursor = false;

#ifdef ENABLE_VR
        std::unique_ptr<VR::Interface> vrInterface = nullptr;
        std::unique_ptr<VR::Session> vrSession = nullptr;
#endif

        VulkanDriver vkDriver;
        std::unique_ptr<ResourceAllocator> resourceAllocator;
        VulkanRenderer renderer;
        std::uint32_t lastFrameIndex = 0;
        std::uint32_t frames = 0;
        std::uint32_t swapchainImageIndexRightNow = 0;


        vk::UniqueCommandPool tracyCommandPool{};
        std::vector<vk::CommandBuffer> tracyCommandBuffers{};

        std::vector<vk::CommandBuffer> mainCommandBuffers{};
        std::vector<vk::CommandBuffer> gBufferCommandBuffers{};
        std::vector<vk::CommandBuffer> gResolveCommandBuffers{};
        std::vector<vk::CommandBuffer> skyboxCommandBuffers{};
        std::vector<vk::UniqueSemaphore> imageAvailableSemaphore{};
        std::vector<vk::UniqueSemaphore> renderFinishedSemaphore{};
        std::vector<vk::UniqueFence> inFlightFences{};
        std::vector<vk::UniqueFence> imagesInFlight{};

        std::list<Carrot::Render::Viewport> viewports;

        bool framebufferResized = false;

        Skybox::Type currentSkybox = Skybox::Type::None;
        std::unique_ptr<Render::Texture> loadedSkyboxTexture = nullptr;
        std::unique_ptr<Mesh> skyboxMesh = nullptr;

        std::unique_ptr<Mesh> screenQuad = nullptr;

        struct ImGuiTextures {
            Render::Texture* allChannels = nullptr;
            Render::Texture* albedo = nullptr;
            Render::Texture* position = nullptr;
            Render::Texture* normal = nullptr;
            Render::Texture* depth = nullptr;
            Render::Texture* raytracing = nullptr;
            Render::Texture* transparent = nullptr;
            Render::Texture* ui = nullptr;
            Render::Texture* intProperties = nullptr;
        };

        std::vector<ImGuiTextures> imguiTextures;

        Carrot::Render::PassData::GResolve gResolvePassData;
        std::unique_ptr<Render::Graph> globalFrameGraph = nullptr;
        std::unique_ptr<Render::Graph> leftEyeGlobalFrameGraph = nullptr;
        std::unique_ptr<Render::Graph> rightEyeGlobalFrameGraph = nullptr;

        std::unordered_map<Render::Eye, std::unique_ptr<Render::Composer>> composers;
        Async::DeferringAwaiter nextFrameAwaiter;

        /// Init engine
        void init();

        /// Init window
        void initWindow();

        /// Init Vulkan for rendering
        void initVulkan();

        /// Init ingame console
        void initConsole();

        /// Create the primary command buffers for rendering
        void recordMainCommandBuffer(std::size_t frameIndex);

        /// Acquires a swapchain image, prepares UBOs, submit command buffer, and present to screen
        void drawFrame(std::size_t currentFrame);

        /// Update the game systems
        void tick(double deltaTime);

        void takeScreenshot();

        /// Create fences and semaphores used for rendering
        void createSynchronizationObjects();

        void recreateSwapchain();

        void initGame();

        void createCameras();

        void createTracyContexts();

        void allocateGraphicsCommandBuffers();

        void updateImGuiTextures(std::size_t swapchainLength);

    private:
        struct Vec2KeyState {
            bool up = false;
            bool left = false;
            bool down = false;
            bool right = false;

            auto operator<=>(const Vec2KeyState&) const = default;

            [[nodiscard]] glm::vec2 asVec2() const {
                glm::vec2 res{0.0f};
                if(up)
                    res.y -= 1.0f;
                if(down)
                    res.y += 1.0f;
                if(left)
                    res.x -= 1.0f;
                if(right)
                    res.x += 1.0f;
                return res;
            }
        };
        std::unordered_map<int, GLFWgamepadstate> gamepadStates;
        std::unordered_map<int, GLFWgamepadstate> gamepadStatePreviousFrame;
        std::unordered_map<Carrot::IO::GameInputVectorType, Vec2KeyState> keysVec2States;
        std::unordered_map<Carrot::IO::GameInputVectorType, Vec2KeyState> keysVec2StatesPreviousFrame;
        std::unordered_map<Carrot::UUID, KeyCallback> keyCallbacks;
        std::unordered_map<Carrot::UUID, MouseButtonCallback> mouseButtonCallbacks;
        std::unordered_map<Carrot::UUID, GamepadButtonCallback> gamepadButtonCallbacks;
        std::unordered_map<Carrot::UUID, GamepadAxisCallback> gamepadAxisCallbacks;
        std::unordered_map<Carrot::UUID, GamepadVec2Callback> gamepadVec2Callbacks;
        std::unordered_map<Carrot::UUID, MousePositionCallback> mousePositionCallbacks;
        std::unordered_map<Carrot::UUID, MouseDeltaCallback> mouseDeltaCallbacks;
        std::unordered_map<Carrot::UUID, MouseDeltaCallback> mouseDeltaGrabbedCallbacks;
        std::unordered_map<Carrot::UUID, KeysVec2Callback> keysVec2Callbacks;

        /// Poll state of gamepads, GLFW does not (yet) post events for gamepad inputs
        void pollGamepads();
        void pollKeysVec2();

        void initInputStructures();

        void onGamepadButtonChange(int gamepadID, int buttonID, bool pressed);
        void onGamepadAxisChange(int gamepadID, int buttonID, float newValue, float oldValue);
        void onGamepadVec2Change(int gamepadID, IO::GameInputVectorType vecID, glm::vec2 newValue, glm::vec2 oldValue);
        void onKeysVec2Change(Carrot::IO::GameInputVectorType vecID, glm::vec2 newValue, glm::vec2 oldValue);

    private: // async members
        std::list<std::future<void>> frameTaskFutures;

    private: // game-specific members
        std::unique_ptr<Carrot::CarrotGame> game = nullptr;

    private:
        static Carrot::Engine* instance;
    };
}

#include "Engine.ipp"