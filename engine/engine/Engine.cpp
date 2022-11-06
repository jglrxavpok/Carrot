//
// Created by jglrxavpok on 21/11/2020.
//

#define GLM_FORCE_RADIANS
#include <filesystem>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "engine/Engine.h"
#include <chrono>
#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <core/io/IO.h>
#include <core/async/OSThreads.h>
#include <engine/render/CameraBufferObject.h>
#include <engine/render/shaders/ShaderStages.h>
#include "engine/constants.h"
#include "engine/render/resources/Buffer.h"
#include "engine/render/resources/Image.h"
#include "engine/render/resources/SingleMesh.h"
#include "engine/render/Model.h"
#include "engine/render/resources/Vertex.h"
#include "engine/render/resources/Pipeline.h"
#include "engine/render/InstanceData.h"
#include "engine/render/Camera.h"
#include "engine/render/raytracing/RayTracer.h"
#include "engine/render/raytracing/ASBuilder.h"
#include "engine/render/GBuffer.h"
#include "engine/render/resources/ResourceAllocator.h"
#include "engine/render/resources/Texture.h"
#include "engine/CarrotGame.h"
#include "stb_image_write.h"
#include "LoadingScreen.h"
#include "engine/console/RuntimeOption.hpp"
#include "engine/console/Console.h"
#include "engine/render/RenderGraph.h"
#include "engine/render/TextureRepository.h"
#include "core/Macros.h"
#include "engine/io/actions/ActionSet.h"
#include "core/io/Logging.hpp"
#include "core/io/FileSystemOS.h"
#include "engine/io/actions/ActionDebug.h"
#include "engine/render/Sprite.h"
#include "engine/physics/PhysicsSystem.h"

#include "engine/ecs/systems/System.h"
#include "engine/ecs/systems/ModelRenderSystem.h"
#include "engine/ecs/systems/RigidBodySystem.h"
#include "engine/ecs/systems/SpriteRenderSystem.h"
#include "engine/ecs/systems/SystemHandleLights.h"
#include "engine/ecs/systems/SystemKinematics.h"
#include "engine/ecs/systems/SystemSinPosition.h"
#include "engine/ecs/systems/SystemUpdateAnimatedModelInstance.h"
#include "engine/ecs/systems/CameraSystem.h"
#include "engine/ecs/systems/TextRenderSystem.h"

#include "engine/ecs/components/Component.h"
#include "engine/ecs/components/AnimatedModelInstance.h"
#include "engine/ecs/components/CameraComponent.h"
#include "engine/ecs/components/ForceSinPosition.h"
#include "engine/ecs/components/Kinematics.h"
#include "engine/ecs/components/LuaScriptComponent.h"
#include "engine/ecs/components/LightComponent.h"
#include "engine/ecs/components/ModelComponent.h"
#include "engine/ecs/components/RigidBodyComponent.h"
#include "engine/ecs/components/SpriteComponent.h"
#include "engine/ecs/components/TextComponent.h"
#include "engine/ecs/components/TransformComponent.h"
#include "engine/ecs/systems/LuaSystems.h"

#include "vr/VRInterface.h"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

Carrot::Engine* Carrot::Engine::instance = nullptr;
static Carrot::RuntimeOption showFPS("Engine/Show FPS", false);
static Carrot::RuntimeOption showInputDebug("Engine/Show Inputs", false);

static std::unordered_set<int> activeJoysticks{};

#ifdef USE_LIVEPP
#include <windows.h>
#include "LPP_API.h"
static HMODULE livePP = NULL;
#endif

Carrot::Engine::SetterHack::SetterHack(Carrot::Engine* e) {
    Carrot::Engine::instance = e;
    Carrot::IO::Resource::vfsToUse = &e->vfs;
    auto exePath = Carrot::IO::getExecutablePath();
    e->vfs.addRoot("engine", exePath.parent_path());
    std::filesystem::current_path(exePath.parent_path());
}

Carrot::Engine::Engine(Configuration config): window(WINDOW_WIDTH, WINDOW_HEIGHT, config),
instanceSetterHack(this),
vrInterface(config.runInVR ? std::make_unique<VR::Interface>(*this) : nullptr),
vkDriver(window, config, this, vrInterface.get()),
resourceAllocator(std::move(std::make_unique<ResourceAllocator>(vkDriver))),
renderer(vkDriver, config), screenQuad(std::make_unique<SingleMesh>(
                                                              std::vector<ScreenSpaceVertex> {
                                                                   { { -1, -1} },
                                                                   { { 1, -1} },
                                                                   { { 1, 1} },
                                                                   { { -1, 1} },
                                                              },
                                                              std::vector<uint32_t> {
                                                                   2,1,0,
                                                                   3,2,0,
                                                              })),
    config(config)
    {
    ZoneScoped;
    instance = this;
    changeTickRate(config.tickRate);

#ifdef USE_LIVEPP
    std::filesystem::path livePPPath = std::filesystem::current_path() / "LivePP" / "";
    livePP = lpp::lppLoadAndRegister(livePPPath.wstring().c_str(), "Carrot");

    lpp::lppEnableAllCallingModulesAsync(livePP);
#endif

    if(config.runInVR) {
        vrSession = vrInterface->createSession();
        vkDriver.getTextureRepository().setXRSession(vrSession.get());
    }

    if(config.runInVR) {
        composers[Render::Eye::LeftEye] = std::make_unique<Render::Composer>(vkDriver);
        composers[Render::Eye::RightEye] = std::make_unique<Render::Composer>(vkDriver);
    } else {
        composers[Render::Eye::NoVR] = std::make_unique<Render::Composer>(vkDriver);
    }

    init();
}

void Carrot::Engine::init() {
    initWindow();

    allocateGraphicsCommandBuffers();
    createTracyContexts();

    createViewport(); // main viewport
    getMainViewport().vrCompatible = true;

    // quickly render something on screen
    LoadingScreen screen{*this};
    initVulkan();

    auto addPresentPass = [&](Render::GraphBuilder& mainGraph, const Render::FrameResource& toPresent) {
        mainGraph.addPass<Carrot::Render::PassData::Present>("present",

                                                             [toPresent](Render::GraphBuilder& builder, Render::Pass<Carrot::Render::PassData::Present>& pass, Carrot::Render::PassData::Present& data) {
                                                                 // pass.rasterized = false;
                                                                 data.input = builder.read(toPresent, vk::ImageLayout::eShaderReadOnlyOptimal);
                                                                 data.output = builder.write(builder.getSwapchainImage(), vk::AttachmentLoadOp::eClear, vk::ImageLayout::eColorAttachmentOptimal, vk::ClearColorValue(std::array{0,0,0,0}));
                                                                 // uses ImGui, so no pre-record: pass.prerecordable = false;
                                                                 builder.present(data.output);
                                                             },
                                                             [this](const Render::CompiledPass& pass, const Render::Context& frame, const Carrot::Render::PassData::Present& data, vk::CommandBuffer& cmds) {
                                                                 ZoneScopedN("CPU RenderGraph present");
                                                                 auto& inputTexture = pass.getGraph().getTexture(data.input, frame.swapchainIndex);
                                                                 auto& swapchainTexture = pass.getGraph().getTexture(data.output, frame.swapchainIndex);
                                                                 frame.renderer.fullscreenBlit(pass.getRenderPass(), frame, inputTexture, swapchainTexture, cmds);

                                                                 {
                                                                     std::lock_guard l { getGraphicsQueue().getMutexUnsafe() };
                                                                     ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmds);
                                                                 }

                                                                 //swapchainTexture.assumeLayout(vk::ImageLayout::eUndefined);
                                                                 //frame.renderer.blit(inputTexture, swapchainTexture, cmds);
                                                                 //swapchainTexture.transitionInline(cmds, vk::ImageLayout::ePresentSrcKHR);
                                                             },
                                                             [this](Render::CompiledPass& pass, Carrot::Render::PassData::Present& data) {
                                                                 renderer.initImGuiPass(pass.getRenderPass());
                                                             }
        );
    };

    if(config.runInVR) {
        Render::GraphBuilder leftEyeGraph(vkDriver);
        Render::GraphBuilder mainGraph(vkDriver);
        Render::Composer companionComposer(vkDriver);

        auto leftEyeFinalPass = fillGraphBuilder(leftEyeGraph, Render::Eye::LeftEye);
        auto& rightEyeFinalPass = leftEyeFinalPass;

        Render::GraphBuilder rightEyeGraph = leftEyeGraph; // reuse most textures

        composers[Render::Eye::LeftEye]->add(leftEyeFinalPass.getData().postProcessed);
        auto& leftEyeComposerPass = composers[Render::Eye::LeftEye]->appendPass(leftEyeGraph);

        composers[Render::Eye::RightEye]->add(rightEyeFinalPass.getData().postProcessed);
        auto& rightEyeComposerPass = composers[Render::Eye::RightEye]->appendPass(rightEyeGraph);

        companionComposer.add(leftEyeComposerPass.getData().color, -1.0, 0.0);
        companionComposer.add(rightEyeComposerPass.getData().color, 0.0, 1.0);

        vrSession->setEyeTexturesToPresent(leftEyeComposerPass.getData().color, rightEyeComposerPass.getData().color);

        auto& composerPass = companionComposer.appendPass(mainGraph);

        leftEyeGlobalFrameGraph = std::move(leftEyeGraph.compile());
        rightEyeGlobalFrameGraph = std::move(rightEyeGraph.compile());

       // auto& imguiPass = renderer.addImGuiPass(mainGraph);

        addPresentPass(mainGraph, composerPass.getData().color);

        getMainViewport().setRenderGraph(std::move(mainGraph.compile()));
    } else {
        Render::GraphBuilder mainGraph(vkDriver);

        auto lastPass = fillGraphBuilder(mainGraph);

        composers[Render::Eye::NoVR]->add(lastPass.getData().postProcessed);
        auto& composerPass = composers[Render::Eye::NoVR]->appendPass(mainGraph);
        addPresentPass(mainGraph, composerPass.getData().color);

        getMainViewport().setRenderGraph(std::move(mainGraph.compile()));
    }
    updateImGuiTextures(getSwapchainImageCount());

    initConsole();
    initInputStructures();
}

void Carrot::Engine::initConsole() {
    Console::instance().registerCommands();
}

void Carrot::Engine::initInputStructures() {
    for (int joystickID = 0; joystickID <= GLFW_JOYSTICK_LAST; ++joystickID) {
        if(glfwJoystickPresent(joystickID) && glfwJoystickIsGamepad(joystickID)) {
            activeJoysticks.insert(joystickID);
        }
    }

    if(config.runInVR) {
        vrSession->attachActionSets();
    }
}

void Carrot::Engine::pollGamepads() {
    gamepadStatePreviousFrame = gamepadStates;
    gamepadStates.clear();

    for(int joystickID : activeJoysticks) {
        if(glfwJoystickIsGamepad(joystickID)) {
            bool vec2ToUpdate[static_cast<std::size_t>(Carrot::IO::GameInputVectorType::Count)] = { false };
            bool valid = glfwGetGamepadState(joystickID, &gamepadStates[joystickID]);
            assert(valid);

            // Update button states
            auto& prevState = gamepadStatePreviousFrame[joystickID];
            auto& state = gamepadStates[joystickID];
            for(int buttonID = 0; buttonID <= GLFW_GAMEPAD_BUTTON_LAST; buttonID++) {
                if(state.buttons[buttonID] != prevState.buttons[buttonID]) {
                    onGamepadButtonChange(joystickID, buttonID, state.buttons[buttonID]);
                }
            }

            // Update axis states
            for(int axisID = 0; axisID <= GLFW_GAMEPAD_BUTTON_LAST; axisID++) {
                if(state.axes[axisID] != prevState.axes[axisID]) {
                    onGamepadAxisChange(joystickID, axisID, state.axes[axisID], prevState.axes[axisID]);

                    for(auto vec2Type = static_cast<std::size_t>(Carrot::IO::GameInputVectorType::First); vec2Type < static_cast<std::size_t>(Carrot::IO::GameInputVectorType::Count); vec2Type++) {
                        if(Carrot::IO::InputVectors[vec2Type].isAxisIn(axisID)) {
                            vec2ToUpdate[vec2Type] = true;
                        }
                    }
                }
            }

            // Update vec2 states
            for(auto vec2Type = static_cast<std::size_t>(Carrot::IO::GameInputVectorType::First); vec2Type < static_cast<std::size_t>(Carrot::IO::GameInputVectorType::Count); vec2Type++) {
                if(vec2ToUpdate[vec2Type]) {
                    auto& input = Carrot::IO::InputVectors[vec2Type];
                    glm::vec2 current = { state.axes[input.horizontalAxisID], state.axes[input.verticalAxisID] };
                    glm::vec2 previous = { prevState.axes[input.horizontalAxisID], prevState.axes[input.verticalAxisID] };
                    onGamepadVec2Change(joystickID, static_cast<Carrot::IO::GameInputVectorType>(vec2Type), current, previous);
                }
            }
        }
    }
}

void Carrot::Engine::onGamepadButtonChange(int gamepadID, int buttonID, bool pressed) {
    for(auto& [id, callback] : gamepadButtonCallbacks) {
        callback(gamepadID, buttonID, pressed);
    }
}

void Carrot::Engine::onGamepadAxisChange(int gamepadID, int axisID, float newValue, float oldValue) {
    for(auto& [id, callback] : gamepadAxisCallbacks) {
        callback(gamepadID, axisID, newValue, oldValue);
    }
}

void Carrot::Engine::onGamepadVec2Change(int gamepadID, Carrot::IO::GameInputVectorType vecID, glm::vec2 newValue, glm::vec2 oldValue) {
    for(auto& [id, callback] : gamepadVec2Callbacks) {
        callback(gamepadID, vecID, newValue, oldValue);
    }
}

void Carrot::Engine::onKeysVec2Change(Carrot::IO::GameInputVectorType vecID, glm::vec2 newValue, glm::vec2 oldValue) {
    for(auto& [id, callback] : keysVec2Callbacks) {
        callback(vecID, newValue, oldValue);
    }
}

void Carrot::Engine::pollKeysVec2() {
    // Update vec2 states
    for(auto vec2TypeIndex = static_cast<std::size_t>(Carrot::IO::GameInputVectorType::First); vec2TypeIndex < static_cast<std::size_t>(Carrot::IO::GameInputVectorType::Count); vec2TypeIndex++) {
        auto vec2Type = static_cast<Carrot::IO::GameInputVectorType>(vec2TypeIndex);
        auto& state = keysVec2States[vec2Type];
        auto& prevState = keysVec2StatesPreviousFrame[vec2Type];
        if(prevState != state) {
            onKeysVec2Change(vec2Type, state.asVec2(), prevState.asVec2());
        }
    }

    keysVec2StatesPreviousFrame = keysVec2States;
}

void Carrot::Engine::run() {
    size_t currentFrame = 0;


    auto previous = std::chrono::steady_clock::now();
    auto lag = std::chrono::duration<float>(0.0f);
    bool ticked = false;
    while(running) {
#ifdef USE_LIVEPP
        lpp::lppSyncPoint(livePP);
#endif

        auto frameStartTime = std::chrono::steady_clock::now();
        std::chrono::duration<float> timeElapsed = frameStartTime-previous;
        currentFPS = 1.0f / timeElapsed.count();
        lag += timeElapsed;
        previous = frameStartTime;

        if(config.runInVR) {
            ZoneScopedN("VR poll events");
            vrInterface->pollEvents();
        }

        {
            ZoneScopedN("File watching");
            Carrot::removeIf(fileWatchers, [](auto p) { return p.expired(); });
            for(const auto& ref : fileWatchers) {
                if(auto ptr = ref.lock()) {
                    ptr->tick();
                }
            }
        }

        if(glfwWindowShouldClose(window.getGLFWPointer())) {
            if(game->onCloseButtonPressed()) {
                game->requestShutdown();
            } else {
                glfwSetWindowShouldClose(window.getGLFWPointer(), false);
            }
        }

        if(game->hasRequestedShutdown()) {
            running = false;
            break;
        }

        {
            ZoneScopedN("Poll inputs");
            glfwPollEvents();
            pollKeysVec2();
            pollGamepads();
            if(config.runInVR) {
                IO::ActionSet::syncXRActions();
            }

            onMouseMove(mouseX, mouseY, true); // Reset input actions based mouse dx/dy
        }

        {
            ZoneScopedN("Setup frame");
            renderer.newFrame();

            {
                ZoneScopedN("ImGui::NewFrame()");
                ImGui::NewFrame();
            }

            {
                ZoneScopedN("nextFrameAwaiter.resume_all()");
                nextFrameAwaiter.resume_all();
            }
        }

        if(showInputDebug) {
            Carrot::IO::debugDrawActions();
        }

        {
            ZoneScopedN("Tick");
            TracyPlot("Tick lag", lag.count());
            TracyPlot("Estimated FPS", currentFPS);
            TracyPlot("Estimated VRAM usage", static_cast<std::int64_t>(Carrot::DeviceMemory::TotalMemoryUsed.load()));
            TracyPlotConfig("Estimated VRAM usage", tracy::PlotFormatType::Memory);

            const std::uint32_t maxCatchupTicks = 10;
            std::uint32_t caughtUp = 0;
            while(lag >= timeBetweenUpdates && caughtUp++ < maxCatchupTicks) {
                Carrot::IO::ActionSet::updatePrePollAllSets(*this);

                GetTaskScheduler().executeMainLoop();
                tick(timeBetweenUpdates.count());
                lag -= timeBetweenUpdates;

                Carrot::IO::ActionSet::resetAllDeltas();
            }
        }

        if(showFPS) {
            if(ImGui::Begin("FPS Counter", nullptr, ImGuiWindowFlags_::ImGuiWindowFlags_NoCollapse)) {
                ImGui::Text("%f FPS", currentFPS);
            }
            ImGui::End();
        }

        drawFrame(currentFrame);
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();

        Carrot::Log::flush();

        nextFrameAwaiter.cleanup();

        currentFrame = (currentFrame+1) % MAX_FRAMES_IN_FLIGHT;

        FrameMark;

        Carrot::Threads::reduceCPULoad();
    }

    glfwHideWindow(window.getGLFWPointer());

    WaitDeviceIdle();
}

void Carrot::Engine::stop() {
    running = false;
}

static void windowResize(GLFWwindow* window, int width, int height) {
    auto app = reinterpret_cast<Carrot::Engine*>(glfwGetWindowUserPointer(window));
    app->onWindowResize();
}

static void mouseMove(GLFWwindow* window, double xpos, double ypos) {
    auto app = reinterpret_cast<Carrot::Engine*>(glfwGetWindowUserPointer(window));
    app->onMouseMove(xpos, ypos, false);
}

static void mouseButton(GLFWwindow* window, int button, int action, int mods) {
    auto app = reinterpret_cast<Carrot::Engine*>(glfwGetWindowUserPointer(window));
    app->onMouseButton(button, action, mods);
}

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    auto app = reinterpret_cast<Carrot::Engine*>(glfwGetWindowUserPointer(window));
    app->onKeyEvent(key, scancode, action, mods);

    if(!app->isGrabbingCursor()) {
        ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
    }
}

static void joystickCallback(int joystickID, int event) {
    if(event == GLFW_CONNECTED) {
        activeJoysticks.insert(joystickID);
    } else if(event == GLFW_DISCONNECTED) {
        activeJoysticks.erase(joystickID);
    }
}

static void scrollCallback(GLFWwindow* window, double xScroll, double yScroll) {
    auto app = reinterpret_cast<Carrot::Engine*>(glfwGetWindowUserPointer(window));
    app->onScroll(xScroll, yScroll);

    if(!app->isGrabbingCursor()) {
        ImGui_ImplGlfw_ScrollCallback(window, xScroll, yScroll);
    }
}

void Carrot::Engine::initWindow() {
    glfwSetWindowUserPointer(window.getGLFWPointer(), this);
    glfwSetFramebufferSizeCallback(window.getGLFWPointer(), windowResize);

    glfwSetCursorPosCallback(window.getGLFWPointer(), mouseMove);
    glfwSetMouseButtonCallback(window.getGLFWPointer(), mouseButton);
    glfwSetKeyCallback(window.getGLFWPointer(), keyCallback);
    glfwSetScrollCallback(window.getGLFWPointer(), scrollCallback);
    glfwSetJoystickCallback(joystickCallback);
}

void Carrot::Engine::initVulkan() {
    renderer.lateInit();

    createCameras();

    initECS();
    initGame();

    createSynchronizationObjects();
}

void Carrot::Engine::initECS() {
    auto& components = Carrot::ECS::getComponentLibrary();
    auto& systems = Carrot::ECS::getSystemLibrary();

    {
        components.add<Carrot::ECS::TransformComponent>();
        components.add<Carrot::ECS::Kinematics>();
        components.add<Carrot::ECS::SpriteComponent>();
        components.add<Carrot::ECS::ModelComponent>();
        //lib.addUniquePtrBased<Carrot::ECS::AnimatedModelInstance>(); // TODO: reintroduce once animated models are reintroduced
        components.add<Carrot::ECS::ForceSinPosition>();
        components.add<Carrot::ECS::LightComponent>();
        components.add<Carrot::ECS::RigidBodyComponent>();
        components.add<Carrot::ECS::CameraComponent>();
        components.add<Carrot::ECS::TextComponent>();
        components.add<Carrot::ECS::LuaScriptComponent>();
    }

    {
        systems.addUniquePtrBased<Carrot::ECS::ModelRenderSystem>();
        systems.addUniquePtrBased<Carrot::ECS::RigidBodySystem>();
        systems.addUniquePtrBased<Carrot::ECS::SpriteRenderSystem>();
        systems.addUniquePtrBased<Carrot::ECS::SystemHandleLights>();
        systems.addUniquePtrBased<Carrot::ECS::SystemKinematics>();
        systems.addUniquePtrBased<Carrot::ECS::SystemSinPosition>();
        systems.addUniquePtrBased<Carrot::ECS::SystemUpdateAnimatedModelInstance>();
        systems.addUniquePtrBased<Carrot::ECS::CameraSystem>();
        systems.addUniquePtrBased<Carrot::ECS::TextRenderSystem>();

        systems.addUniquePtrBased<Carrot::ECS::LuaRenderSystem>();
        systems.addUniquePtrBased<Carrot::ECS::LuaUpdateSystem>();
    }
}

Carrot::Engine::~Engine() {
    Carrot::Render::Sprite::cleanup();
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    for(auto& ctx : tracyCtx) {
        TracyVkDestroy(ctx);
    }
    tracyCtx.clear();

#ifdef USE_LIVEPP
    lpp::lppShutdown(livePP);
    ::FreeLibrary(livePP);
#endif
/*    for(size_t i = 0; i < getSwapchainImageCount(); i++) {
        TracyVkDestroy(tracyCtx[i]);
    }*/
}

std::unique_ptr<Carrot::CarrotGame>& Carrot::Engine::getGame() {
    return game;
}

void Carrot::Engine::recordMainCommandBuffer(size_t i) {
    // main command buffer
    vk::CommandBufferBeginInfo beginInfo{
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
            .pInheritanceInfo = nullptr,
    };

    {
        {
            ZoneScopedN("ImGui Render");
            Console::instance().renderToImGui(*this);
            ImGui::Render();
        }

        {
            ZoneScopedN("mainCommandBuffers[i].begin(beginInfo)");
            mainCommandBuffers[i].begin(beginInfo);

            GetVulkanDriver().setMarker(mainCommandBuffers[i], "begin command buffer");
        }

        TracyVkZone(tracyCtx[i], mainCommandBuffers[i], "Main command buffer");

        if(config.runInVR) {
            {
                ZoneScopedN("VR Left eye render");
                GetVulkanDriver().setMarker(mainCommandBuffers[i], "VR Left eye render");
                leftEyeGlobalFrameGraph->execute(newRenderContext(i, getMainViewport(), Render::Eye::LeftEye), mainCommandBuffers[i]);
            }
            {
                ZoneScopedN("VR Right eye render");
                GetVulkanDriver().setMarker(mainCommandBuffers[i], "VR Right eye render");
                rightEyeGlobalFrameGraph->execute(newRenderContext(i, getMainViewport(), Render::Eye::RightEye), mainCommandBuffers[i]);
            }
        }

        {
            GetVulkanDriver().setMarker(mainCommandBuffers[i], "render viewports");
            ZoneScopedN("Render viewports");
            for(auto it = viewports.rbegin(); it != viewports.rend(); it++) {
                ZoneScopedN("Render single viewport");
                auto& viewport = *it;
                GetVulkanDriver().setFormattedMarker(mainCommandBuffers[i], "render viewport %x", &viewport);
                if(config.runInVR) {
                    // each eye is done in separate render passes above this code
                    viewport.render(newRenderContext(i, viewport, Render::Eye::NoVR), mainCommandBuffers[i]);
                } else {
                    viewport.render(newRenderContext(i, viewport, Render::Eye::NoVR), mainCommandBuffers[i]);
                }
            }
        }

        TracyVkCollect(tracyCtx[i], mainCommandBuffers[i]);
    }

    mainCommandBuffers[i].end();
}

void Carrot::Engine::allocateGraphicsCommandBuffers() {
    vk::CommandBufferAllocateInfo allocInfo{
            .commandPool = getGraphicsCommandPool(),
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = static_cast<uint32_t>(getSwapchainImageCount()),
    };

    this->mainCommandBuffers = this->getLogicalDevice().allocateCommandBuffers(allocInfo);

    vk::CommandBufferAllocateInfo gAllocInfo {
            .commandPool = getGraphicsCommandPool(),
            .level = vk::CommandBufferLevel::eSecondary,
            .commandBufferCount = static_cast<uint32_t>(getSwapchainImageCount()),
    };
    this->gBufferCommandBuffers = getLogicalDevice().allocateCommandBuffers(gAllocInfo);
    this->gResolveCommandBuffers = getLogicalDevice().allocateCommandBuffers(gAllocInfo);
    this->skyboxCommandBuffers = getLogicalDevice().allocateCommandBuffers(gAllocInfo);
}

void Carrot::Engine::drawFrame(size_t currentFrame) {
    ZoneScoped;

    vk::Result result;
    uint32_t imageIndex;
    {
        ZoneNamedN(__acquire, "Acquire image", true);

        {
            ZoneNamedN(__fences, "Wait fences", true);
            try {
                static_cast<void>(getLogicalDevice().waitForFences((*inFlightFences[currentFrame]), true, UINT64_MAX));
            } catch(vk::DeviceLostError& deviceLost) {
                GetVulkanDriver().onDeviceLost();
            }
            getLogicalDevice().resetFences((*inFlightFences[currentFrame]));
        }

        //TracyVulkanCollect(tracyCtx[lastFrameIndex]);

        {
            ZoneScopedN("acquireNextImageKHR");
            auto nextImage = getLogicalDevice().acquireNextImageKHR(vkDriver.getSwapchain(), UINT64_MAX,
                                                                    *imageAvailableSemaphore[currentFrame], nullptr);
            result = nextImage.result;

            if (result == vk::Result::eErrorOutOfDateKHR) {
                recreateSwapchain();
                return;
            } else if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
                throw std::runtime_error("Failed to acquire swap chain image");
            }
            imageIndex = nextImage.value;
            swapchainImageIndexRightNow = imageIndex;
        }
    }

    Carrot::Render::Context mainRenderContext = newRenderContext(imageIndex, getMainViewport());
    vkDriver.newFrame(mainRenderContext);

    {
        ZoneScopedN("Prepare frame");

        if(config.runInVR) {
            ZoneScopedN("VR start frame");
            vrSession->startFrame();
        }

        vkDriver.startFrame(mainRenderContext);
        renderer.beginFrame(mainRenderContext);
        GetTaskScheduler().executeRendering();

        auto onFrame = [&](Carrot::Render::Viewport& v) {
            ZoneScoped;
            std::string profilingMarker = Carrot::sprintf("Viewport %x", &v);
            ZoneText(profilingMarker.c_str(), profilingMarker.size());
            Carrot::Render::Context renderContext = newRenderContext(imageIndex, v);
            {
                ZoneScopedN("game->onFrame");
                if(config.runInVR) {
                    game->setupCamera(newRenderContext(imageIndex, v, Carrot::Render::Eye::LeftEye));
                    game->setupCamera(newRenderContext(imageIndex, v, Carrot::Render::Eye::RightEye));
                    game->onFrame(newRenderContext(imageIndex, v));
                } else {
                    game->setupCamera(renderContext);
                    game->onFrame(renderContext);
                }
            }
            GetPhysics().onFrame(renderContext);
            getRayTracer().onFrame(renderContext); // update instance positions only once everything has been updated
            v.onFrame(renderContext); // update cameras only once all render systems are updated
            renderer.onFrame(renderContext);
        };
        for(auto& v : viewports) {
            if(&v == &getMainViewport()) {
                continue;
            }

            onFrame(v);
        }
        onFrame(getMainViewport());
        renderer.endFrame(mainRenderContext);
    }
    {
        ZoneScopedN("Record main command buffer");
        recordMainCommandBuffer(imageIndex);
    }

    {
        ZoneScopedN("Present");

        std::vector<vk::Semaphore> waitSemaphores = {*imageAvailableSemaphore[currentFrame]};
        std::vector<vk::PipelineStageFlags> waitStages = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
        vk::Semaphore signalSemaphores[] = {*renderFinishedSemaphore[currentFrame]};

        game->changeGraphicsWaitSemaphores(imageIndex, waitSemaphores, waitStages);

        for(auto [stage, semaphore] : additionalWaitSemaphores) {
            waitSemaphores.push_back(semaphore);
            waitStages.push_back(stage);
        }
        additionalWaitSemaphores.clear();

        vk::SubmitInfo submitInfo{
                .waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size()),
                .pWaitSemaphores = waitSemaphores.data(),

                .pWaitDstStageMask = waitStages.data(),

                .commandBufferCount = 1,
                .pCommandBuffers = &mainCommandBuffers[imageIndex],

                .signalSemaphoreCount = 1,
                .pSignalSemaphores = signalSemaphores,
        };

        {
            ZoneScopedN("Renderer Pre-Frame actions");
            renderer.preFrame();
        }

        {
            ZoneScopedN("Reset in flight fences");
            getLogicalDevice().resetFences(*inFlightFences[currentFrame]);
        }
        {
            ZoneScopedN("Submit to graphics queue");
            //presentThread.present(currentFrame, signalSemaphores[0], submitInfo, *inFlightFences[currentFrame]);

            waitForFrameTasks();

            GetVulkanDriver().submitGraphics(submitInfo, *inFlightFences[currentFrame]);
            vk::SwapchainKHR swapchains[] = { vkDriver.getSwapchain() };

            vk::PresentInfoKHR presentInfo{
                    .waitSemaphoreCount = 1,
                    .pWaitSemaphores = signalSemaphores,

                    .swapchainCount = 1,
                    .pSwapchains = swapchains,
                    .pImageIndices = &imageIndex,
                    .pResults = nullptr,
            };

            {
                ZoneScopedN("PresentKHR");
                vkDriver.getPresentQueue().presentKHR(presentInfo);
            }
        }

        if(config.runInVR) {
            {
                ZoneScopedN("VR render");
                vrSession->present(newRenderContext(imageIndex, getMainViewport()));
            }
        }

        {
            ZoneScopedN("Renderer Post-Frame actions");
            renderer.postFrame();
        }
    }

    lastFrameIndex = imageIndex;

    if(framebufferResized) {
        recreateSwapchain();
    }
    frames++;
}

void Carrot::Engine::createSynchronizationObjects() {
    imageAvailableSemaphore.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphore.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    imagesInFlight.resize(getSwapchainImageCount());

    vk::SemaphoreCreateInfo semaphoreInfo{};

    vk::FenceCreateInfo fenceInfo{
            .flags = vk::FenceCreateFlagBits::eSignaled,
    };

    for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        imageAvailableSemaphore[i] = getLogicalDevice().createSemaphoreUnique(semaphoreInfo, vkDriver.getAllocationCallbacks());
        renderFinishedSemaphore[i] = getLogicalDevice().createSemaphoreUnique(semaphoreInfo, vkDriver.getAllocationCallbacks());
        inFlightFences[i] = getLogicalDevice().createFenceUnique(fenceInfo, vkDriver.getAllocationCallbacks());
    }
}

void Carrot::Engine::recreateSwapchain() {
    vkDriver.fetchNewFramebufferSize();

    framebufferResized = false;

    WaitDeviceIdle();

    std::size_t previousImageCount = getSwapchainImageCount();
    vkDriver.cleanupSwapchain();
    vkDriver.createSwapChain();

    // TODO: only recreate if necessary
    if(previousImageCount != vkDriver.getSwapchainImageCount()) {
        onSwapchainImageCountChange(vkDriver.getSwapchainImageCount());
    }
    onSwapchainSizeChange(vkDriver.getFinalRenderSize().width, vkDriver.getFinalRenderSize().height);
}

void Carrot::Engine::onWindowResize() {
    framebufferResized = true;
}

const Carrot::QueueFamilies& Carrot::Engine::getQueueFamilies() {
    return vkDriver.getQueueFamilies();
}

vk::Device& Carrot::Engine::getLogicalDevice() {
    return vkDriver.getLogicalDevice();
}

vk::Optional<const vk::AllocationCallbacks> Carrot::Engine::getAllocator() {
    return vkDriver.getAllocationCallbacks();
}

vk::CommandPool& Carrot::Engine::getTransferCommandPool() {
    return vkDriver.getThreadTransferCommandPool();
}

vk::CommandPool& Carrot::Engine::getGraphicsCommandPool() {
    return vkDriver.getThreadGraphicsCommandPool();
}

vk::CommandPool& Carrot::Engine::getComputeCommandPool() {
    return vkDriver.getThreadComputeCommandPool();
}

Carrot::Vulkan::SynchronizedQueue& Carrot::Engine::getTransferQueue() {
    return vkDriver.getTransferQueue();
}

Carrot::Vulkan::SynchronizedQueue& Carrot::Engine::getGraphicsQueue() {
    return vkDriver.getGraphicsQueue();
}

Carrot::Vulkan::SynchronizedQueue& Carrot::Engine::getPresentQueue() {
    return vkDriver.getPresentQueue();
}

std::set<std::uint32_t> Carrot::Engine::createGraphicsAndTransferFamiliesSet() {
    return vkDriver.createGraphicsAndTransferFamiliesSet();
}

std::uint32_t Carrot::Engine::getSwapchainImageCount() {
    return vkDriver.getSwapchainImageCount();
}

void Carrot::Engine::createCameras() {
    auto center = glm::vec3(5*0.5f, 5*0.5f, 0);

    if(config.runInVR) {
        getMainViewport().getCamera(Render::Eye::LeftEye) = Camera(glm::mat4{1.0f}, glm::mat4{1.0f});
        getMainViewport().getCamera(Render::Eye::RightEye) = Camera(glm::mat4{1.0f}, glm::mat4{1.0f});
    } else {
        auto camera = Camera(45.0f, vkDriver.getWindowFramebufferExtent().width / (float) vkDriver.getWindowFramebufferExtent().height, 0.1f, 1000.0f);
        camera.getPositionRef() = glm::vec3(center.x, center.y + 1, 5.0f);
        camera.getTargetRef() = center;
        getMainViewport().getCamera(Render::Eye::NoVR) = std::move(camera);
    }
}

void Carrot::Engine::onMouseMove(double xpos, double ypos, bool updateOnlyDelta) {
    double dx = xpos-mouseX;
    double dy = ypos-mouseY;
    for(auto& [id, callback] : mouseDeltaCallbacks) {
        callback(dx, dy);
    }
    if(grabbingCursor) {
        for(auto& [id, callback] : mouseDeltaGrabbedCallbacks) {
            callback(dx, dy);
        }
    }
    if(!updateOnlyDelta) {
        for(auto& [id, callback] : mousePositionCallbacks) {
            callback(xpos, ypos);
        }
        if(game) {
            game->onMouseMove(dx, dy);
        }
        mouseX = xpos;
        mouseY = ypos;
    }
}

Carrot::Camera& Carrot::Engine::getCamera() {
    return getMainViewportCamera(Carrot::Render::Eye::NoVR);
}

Carrot::Camera& Carrot::Engine::getMainViewportCamera(Carrot::Render::Eye eye) {
    return getMainViewport().getCamera(eye);
}

void Carrot::Engine::onMouseButton(int button, int action, int mods) {
    for(auto& [id, callback] : mouseButtonCallbacks) {
        callback(button, action == GLFW_PRESS || action == GLFW_REPEAT, mods);
    }
}

void Carrot::Engine::onKeyEvent(int key, int scancode, int action, int mods) {
    if(key == GLFW_KEY_GRAVE_ACCENT && action == GLFW_RELEASE) {
        Console::instance().toggleVisibility();
    }

    if(key == GLFW_KEY_F2 && action == GLFW_PRESS) {
        takeScreenshot();
    }

    for(auto& [id, callback] : keyCallbacks) {
        callback(key, scancode, action, mods);
    }

    if(action == GLFW_REPEAT)
        return;
    bool pressed = action == GLFW_PRESS;
    for(auto vec2TypeIndex = static_cast<std::size_t>(Carrot::IO::GameInputVectorType::First); vec2TypeIndex < static_cast<std::size_t>(Carrot::IO::GameInputVectorType::Count); vec2TypeIndex++) {
        auto& input = Carrot::IO::InputVectors[vec2TypeIndex];
        if(input.isButtonIn(key)) {
            auto vec2Type = static_cast<Carrot::IO::GameInputVectorType>(vec2TypeIndex);
            auto& state = keysVec2States[vec2Type];
            if(input.upKey == key)
                state.up = pressed;
            if(input.leftKey == key)
                state.left = pressed;
            if(input.rightKey == key)
                state.right = pressed;
            if(input.downKey == key)
                state.down = pressed;
        }
    }

            /*
            if(key == GLFW_KEY_G && action == GLFW_PRESS) {
                grabCursor = !grabCursor;
                glfwSetInputMode(window.get(), GLFW_CURSOR, grabCursor ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
            }*/



    // TODO: pass input to game
}

void Carrot::Engine::onScroll(double xScroll, double yScroll) {
    // TODO: expose to input API
}

void Carrot::Engine::createTracyContexts() {
    vk::DynamicLoader dl;
    tracyCtx = std::vector<TracyVkCtx>{ getSwapchainImageCount(), nullptr };
    PFN_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT ptr_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT = dl.getProcAddress<PFN_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT>("vkGetPhysicalDeviceCalibrateableTimeDomainsEXT");
    PFN_vkGetCalibratedTimestampsEXT ptr_vkGetCalibratedTimestampsEXT = dl.getProcAddress<PFN_vkGetCalibratedTimestampsEXT>("vkGetCalibratedTimestampsEXT");

    for(size_t i = 0; i < getSwapchainImageCount(); i++) {
        //tracyCtx.emplace_back(std::move(std::make_unique<TracyVulkanContext>(vkDriver.getPhysicalDevice(), getLogicalDevice(), getGraphicsQueue().getQueueUnsafe(), getQueueFamilies().graphicsFamily.value())));
        if(ptr_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT != nullptr && ptr_vkGetCalibratedTimestampsEXT != nullptr) {
            tracyCtx[i] = TracyVkContextCalibrated(vkDriver.getPhysicalDevice(), getLogicalDevice(), getGraphicsQueue().getQueueUnsafe(), mainCommandBuffers[i], ptr_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT, ptr_vkGetCalibratedTimestampsEXT);
        } else {
            tracyCtx[i] = TracyVkContext(vkDriver.getPhysicalDevice(), getLogicalDevice(), getGraphicsQueue().getQueueUnsafe(), mainCommandBuffers[i]);
        }
    }
}

Carrot::Vulkan::SynchronizedQueue& Carrot::Engine::getComputeQueue() {
    return vkDriver.getComputeQueue();
}

Carrot::ASBuilder& Carrot::Engine::getASBuilder() {
    return renderer.getASBuilder();
}

void Carrot::Engine::tick(double deltaTime) {
    ZoneScoped;
    game->tick(deltaTime);
    GetPhysics().tick(deltaTime);
}

void Carrot::Engine::takeScreenshot() {
    namespace fs = std::filesystem;

    auto currentTime = std::chrono::system_clock::now().time_since_epoch().count();
    auto screenshotFolder = fs::current_path() / "screenshots";
    if(!fs::exists(screenshotFolder)) {
        if(!fs::create_directories(screenshotFolder)) {
            throw std::runtime_error("Could not create screenshot folder");
        }
    }
    auto screenshotPath = screenshotFolder / (std::to_string(currentTime) + ".png");

    auto& lastImage = vkDriver.getSwapchainTextures()[lastFrameIndex];

    auto& swapchainExtent = vkDriver.getFinalRenderSize();
    auto screenshotImage = Image(vkDriver,
                                 {swapchainExtent.width, swapchainExtent.height, 1},
                                 vk::ImageUsageFlagBits::eTransferDst,
                                 vk::Format::eR8G8B8A8Unorm
                                 );

    vk::DeviceSize bufferSize = 4*swapchainExtent.width*swapchainExtent.height * sizeof(uint32_t); // TODO
    auto screenshotBuffer = getResourceAllocator().allocateBuffer(
                                   bufferSize,
                                   vk::BufferUsageFlagBits::eTransferDst,
                                   vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
                                   );

    auto offsetMin = vk::Offset3D {
            .x = 0,
            .y = 0,
            .z = 0,
    };
    auto offsetMax = vk::Offset3D {
            .x = static_cast<int32_t>(swapchainExtent.width),
            .y = static_cast<int32_t>(swapchainExtent.height),
            .z = 1,
    };
    performSingleTimeGraphicsCommands([&](vk::CommandBuffer& commands) {
        lastImage->assumeLayout(vk::ImageLayout::ePresentSrcKHR);
        lastImage->transitionInline(commands, vk::ImageLayout::eTransferSrcOptimal);
        screenshotImage.transitionLayoutInline(commands, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

        commands.blitImage(lastImage->getImage().getVulkanImage(), vk::ImageLayout::eTransferSrcOptimal, screenshotImage.getVulkanImage(), vk::ImageLayout::eTransferDstOptimal, vk::ImageBlit {
                .srcSubresource = {
                        .aspectMask = vk::ImageAspectFlagBits::eColor,
                        .mipLevel = 0,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                },
                .srcOffsets = std::array<vk::Offset3D, 2> {
                        offsetMin,
                        offsetMax,
                },
                .dstSubresource = {
                        .aspectMask = vk::ImageAspectFlagBits::eColor,
                        .mipLevel = 0,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                },
                .dstOffsets = std::array<vk::Offset3D, 2> {
                        offsetMin,
                        offsetMax,
                },
        }, vk::Filter::eNearest);

        commands.copyImageToBuffer(screenshotImage.getVulkanImage(), vk::ImageLayout::eGeneral, screenshotBuffer.getVulkanBuffer(), vk::BufferImageCopy {
            // tightly packed
            .bufferRowLength = 0,
            .bufferImageHeight = 0,

            .imageSubresource = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .mipLevel = 0,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
            },

            .imageExtent = {
                    .width = swapchainExtent.width,
                    .height = swapchainExtent.height,
                    .depth = 1,
            },
        });
    });

    void* pData = screenshotBuffer.map<void>();
    stbi_write_png(screenshotPath.generic_string().c_str(), swapchainExtent.width, swapchainExtent.height, 4, pData, 4 * swapchainExtent.width);

    screenshotBuffer.unmap();
}

Carrot::Skybox::Type Carrot::Engine::getSkybox() const {
    return currentSkybox;
}

void Carrot::Engine::setSkybox(Carrot::Skybox::Type type) {
    static std::vector<SimpleVertex> skyboxVertices = {
            { { 1.0f, -1.0f, -1.0f } },
            { { 1.0f, -1.0f, 1.0f } },
            { { -1.0f, -1.0f, -1.0f } },
            { { -1.0f, -1.0f, 1.0f } },
            { { 1.0f, 1.0f, -1.0f } },
            { { 1.0f, 1.0f, 1.0f } },
            { { -1.0f, 1.0f, -1.0f } },
            { { -1.0f, 1.0f, 1.0f } },
    };
    static std::vector<std::uint32_t> skyboxIndices = {
            1, 2, 0,
            3, 6, 2,
            7, 4, 6,
            5, 0, 4,
            6, 0, 2,
            3, 5, 7,
            1, 3, 2,
            3, 7, 6,
            7, 5, 4,
            5, 1, 0,
            6, 4, 0,
            3, 1, 5,
    };
    currentSkybox = type;
    if(type != Carrot::Skybox::Type::None) {
        ZoneScopedN("Prepare skybox texture & mesh");
        {
            ZoneScopedN("Load skybox cubemap");
            loadedSkyboxTexture = std::make_unique<Render::Texture>(Image::cubemapFromFiles(vkDriver, [type](Skybox::Direction dir) {
                return Skybox::getTexturePath(type, dir);
            }));
            loadedSkyboxTexture->name("Current loaded skybox");
        }

        {
            ZoneScopedN("Create skybox mesh");
            skyboxMesh = make_unique<SingleMesh>(skyboxVertices, skyboxIndices);
        }
    }
}

void Carrot::Engine::onSwapchainImageCountChange(size_t newCount) {
    vkDriver.onSwapchainImageCountChange(newCount);

    // TODO: rebuild graphs
    // TODO: multi-threading (command pools are threadlocal)
    vkDriver.getLogicalDevice().resetCommandPool(getGraphicsCommandPool());
    allocateGraphicsCommandBuffers();

    renderer.onSwapchainImageCountChange(newCount);

    if(config.runInVR) {
        leftEyeGlobalFrameGraph->onSwapchainImageCountChange(newCount);
        rightEyeGlobalFrameGraph->onSwapchainImageCountChange(newCount);
    }
    for(auto& v : viewports) {
        v.onSwapchainImageCountChange(newCount);
    }

    createSynchronizationObjects();

    game->onSwapchainImageCountChange(newCount);

    updateImGuiTextures(newCount);
}

void Carrot::Engine::onSwapchainSizeChange(int newWidth, int newHeight) {
    vkDriver.onSwapchainSizeChange(newWidth, newHeight);

    renderer.onSwapchainSizeChange(newWidth, newHeight);

    if(config.runInVR) {
        leftEyeGlobalFrameGraph->onSwapchainSizeChange(newWidth, newHeight);
        rightEyeGlobalFrameGraph->onSwapchainSizeChange(newWidth, newHeight);
    }
    getMainViewport().onSwapchainSizeChange(newWidth, newHeight);
    for(auto& v : viewports) {
        if(&v == &getMainViewport()) {
            continue;
        }
        v.onSwapchainSizeChange(newWidth, newHeight);
    }

    game->onSwapchainSizeChange(newWidth, newHeight);

    updateImGuiTextures(getSwapchainImageCount());
}

void Carrot::Engine::updateImGuiTextures(std::size_t swapchainLength) {
    imguiTextures.resize(swapchainLength);
    verify(getMainViewport().getRenderGraph() != nullptr, "No render graph associated to main viewport");
    /* TODO: Do I still want GBuffer debugging? -> or at least redo it
    for (int i = 0; i < swapchainLength; ++i) {
        auto& textures = imguiTextures[i];
        textures.allChannels = &getMainViewport().getRenderGraph()->getTexture(gResolvePassData.resolved, i);

        textures.albedo = &getMainViewport().getRenderGraph()->getTexture(gResolvePassData.albedo, i);

        textures.position = &getMainViewport().getRenderGraph()->getTexture(gResolvePassData.positions, i);

        textures.normal = &getMainViewport().getRenderGraph()->getTexture(gResolvePassData.normals, i);

        textures.depth = &getMainViewport().getRenderGraph()->getTexture(gResolvePassData.depthStencil, i);

        textures.intProperties = &getMainViewport().getRenderGraph()->getTexture(gResolvePassData.flags, i);

        textures.transparent = &getMainViewport().getRenderGraph()->getTexture(gResolvePassData.transparent, i);
    }*/
}

Carrot::Render::Context Carrot::Engine::newRenderContext(std::size_t swapchainFrameIndex, Carrot::Render::Viewport& viewport, Carrot::Render::Eye eye) {
    return Carrot::Render::Context {
            .renderer = renderer,
            .viewport = viewport,
            .eye = eye,
            .frameCount = frames,
            .swapchainIndex = swapchainFrameIndex,
            .lastSwapchainIndex = lastFrameIndex,
    };
}

Carrot::VR::Session& Carrot::Engine::getVRSession() {
    verify(config.runInVR, "Cannot access VR Session if not running in VR");
    verify(vrSession, "VR session is null");
    return *vrSession;
}

Carrot::Async::Task<> Carrot::Engine::cowaitNextFrame() {
    co_await nextFrameAwaiter;
}

void Carrot::Engine::addFrameTask(FrameTask&& task) {
    frameTaskFutures.emplace_back(std::move(std::async(std::launch::async, task)));
}

void Carrot::Engine::waitForFrameTasks() {
    for(auto& f : frameTaskFutures) {
        f.wait();
    }
    frameTaskFutures.clear();
}

Carrot::Render::Viewport& Carrot::Engine::getMainViewport() {
    return viewports.front();
}

Carrot::Render::Viewport& Carrot::Engine::createViewport() {
    viewports.emplace_back(renderer);
    return viewports.back();
}

Carrot::Render::Pass<Carrot::Render::PassData::PostProcessing>& Carrot::Engine::fillInDefaultPipeline(Carrot::Render::GraphBuilder& mainGraph, Carrot::Render::Eye eye,
                                                                                                std::function<void(const Carrot::Render::CompiledPass&,
                                                              const Carrot::Render::Context&,
                                                              vk::CommandBuffer&)> opaqueCallback,
                                                                                                std::function<void(const Carrot::Render::CompiledPass&,
                                                              const Carrot::Render::Context&,
                                                              vk::CommandBuffer&)> transparentCallback,
                                                                                                const Render::TextureSize& framebufferSize) {
    auto& skyboxPass = mainGraph.addPass<Carrot::Render::PassData::Skybox>("skybox",
                                                                           [this, framebufferSize](Render::GraphBuilder& builder, Render::Pass<Carrot::Render::PassData::Skybox>& pass, Carrot::Render::PassData::Skybox& data) {
                                                                               data.output = builder.createRenderTarget(vk::Format::eR8G8B8A8Unorm,
                                                                                                                        framebufferSize,
                                                                                                                        vk::AttachmentLoadOp::eClear,
                                                                                                                        vk::ClearColorValue(std::array{0,0,0,0})
                                                                               );
                                                                           },
                                                                           [this](const Render::CompiledPass& pass, const Render::Context& frame, const Carrot::Render::PassData::Skybox& data, vk::CommandBuffer& buffer) {
                                                                               ZoneScopedN("CPU RenderGraph skybox");
                                                                               auto skyboxPipeline = renderer.getOrCreateRenderPassSpecificPipeline("skybox", pass.getRenderPass());
                                                                               renderer.bindTexture(*skyboxPipeline, frame, *loadedSkyboxTexture, 1, 0, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::eCube);
                                                                               skyboxPipeline->bind(pass.getRenderPass(), frame, buffer);
                                                                               skyboxMesh->bind(buffer);
                                                                               skyboxMesh->draw(buffer);
                                                                           }
    );
    skyboxPass.setCondition([this](const Render::CompiledPass& pass, const Render::Context& frame, const Carrot::Render::PassData::Skybox& data) {
        return currentSkybox != Skybox::Type::None;
    });

    auto& opaqueGBufferPass = getGBuffer().addGBufferPass(mainGraph, [opaqueCallback](const Render::CompiledPass& pass, const Render::Context& frame, vk::CommandBuffer& cmds) {
        ZoneScopedN("CPU RenderGraph Opaque GPass");
        opaqueCallback(pass, frame, cmds);
    }, framebufferSize);
    auto& transparentGBufferPass = getGBuffer().addTransparentGBufferPass(mainGraph, opaqueGBufferPass.getData(), [transparentCallback](const Render::CompiledPass& pass, const Render::Context& frame, vk::CommandBuffer& cmds) {
        ZoneScopedN("CPU RenderGraph Opaque GPass");
        transparentCallback(pass, frame, cmds);
    }, framebufferSize);
    auto& lightingPass = getGBuffer().addGResolvePass(opaqueGBufferPass.getData(), transparentGBufferPass.getData(), skyboxPass.getData().output, mainGraph, framebufferSize);

    auto& postProcessPass = mainGraph.addPass<Carrot::Render::PassData::PostProcessing>("post-processing",
                                                                           [this, lightingPass, framebufferSize](Render::GraphBuilder& builder, Render::Pass<Carrot::Render::PassData::PostProcessing>& pass, Carrot::Render::PassData::PostProcessing& data) {
                                                                                data.postLighting = builder.read(lightingPass.getData().resolved, vk::ImageLayout::eShaderReadOnlyOptimal);
                                                                                data.postProcessed = builder.createRenderTarget(vk::Format::eR8G8B8A8Srgb,
                                                                                                                        framebufferSize,
                                                                                                                        vk::AttachmentLoadOp::eClear,
                                                                                                                        vk::ClearColorValue(std::array{0,0,0,0}),
                                                                                                                        vk::ImageLayout::eColorAttachmentOptimal
                                                                                );
                                                                           },
                                                                           [this](const Render::CompiledPass& pass, const Render::Context& frame, const Carrot::Render::PassData::PostProcessing& data, vk::CommandBuffer& buffer) {
                                                                               ZoneScopedN("CPU RenderGraph post-process");
                                                                               TracyVkZone(GetEngine().tracyCtx[frame.swapchainIndex], buffer, "Post-Process");
                                                                               auto pipeline = renderer.getOrCreateRenderPassSpecificPipeline("post-process/tone-mapping", pass.getRenderPass());
                                                                               renderer.bindTexture(*pipeline, frame, pass.getGraph().getTexture(data.postLighting, frame.swapchainIndex), 0, 0, nullptr);

                                                                               renderer.bindSampler(*pipeline, frame, renderer.getVulkanDriver().getNearestSampler(), 0, 1);
                                                                               renderer.bindSampler(*pipeline, frame, renderer.getVulkanDriver().getLinearSampler(), 0, 2);

                                                                               pipeline->bind(pass.getRenderPass(), frame, buffer);
                                                                               auto& screenQuadMesh = frame.renderer.getFullscreenQuad();
                                                                               screenQuadMesh.bind(buffer);
                                                                               screenQuadMesh.draw(buffer);
                                                                           }
    );
    return postProcessPass;
}

Carrot::Render::Pass<Carrot::Render::PassData::PostProcessing>& Carrot::Engine::fillGraphBuilder(Render::GraphBuilder& mainGraph, Render::Eye eye, const Render::TextureSize& framebufferSize) {
    auto& finalPass = fillInDefaultPipeline(mainGraph, eye,
                                               [&](const Render::CompiledPass& pass, const Render::Context& frame, vk::CommandBuffer& cmds) {
                                                   TracyVkZone(tracyCtx[frame.swapchainIndex], cmds, "Opaque Rendering");
                                                   ZoneScopedN("CPU RenderGraph Opaque GPass");
                                                   game->recordOpaqueGBufferPass(pass.getRenderPass(), frame, cmds);
                                                   renderer.recordOpaqueGBufferPass(pass.getRenderPass(), frame, cmds);
                                               },
                                               [&](const Render::CompiledPass& pass, const Render::Context& frame, vk::CommandBuffer& cmds) {
                                                   TracyVkZone(tracyCtx[frame.swapchainIndex], cmds, "Transparent Rendering");
                                                   ZoneScopedN("CPU RenderGraph Transparent GPass");
                                                   game->recordTransparentGBufferPass(pass.getRenderPass(), frame, cmds);
                                                   renderer.recordTransparentGBufferPass(pass.getRenderPass(), frame, cmds);
                                               },
                                               framebufferSize);

    return finalPass;
}

std::shared_ptr<Carrot::IO::FileWatcher> Carrot::Engine::createFileWatcher(const Carrot::IO::FileWatcher::Action& action, const std::vector<std::filesystem::path>& filesToWatch) {
    auto watcher = std::make_shared<Carrot::IO::FileWatcher>(action, filesToWatch);
    fileWatchers.emplace_back(watcher);
    return watcher;
}

Carrot::TaskScheduler& Carrot::Engine::getTaskScheduler() {
    return taskScheduler;
}

void Carrot::Engine::addWaitSemaphoreBeforeRendering(const vk::PipelineStageFlags& stage, const vk::Semaphore& semaphore) {
    additionalWaitSemaphores.push_back({stage, semaphore});
}

void Carrot::Engine::grabCursor() {
    grabbingCursor = true;
    glfwSetInputMode(window.getGLFWPointer(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
}

void Carrot::Engine::ungrabCursor() {
    grabbingCursor = false;
    glfwSetInputMode(window.getGLFWPointer(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
}

void Carrot::Engine::changeTickRate(std::uint32_t tickRate) {
    timeBetweenUpdates = std::chrono::duration<float>(1.0f / tickRate);
}

#ifdef TRACY_ENABLE
void* operator new(std::size_t count) {
    auto ptr = malloc(count);
    TracyAllocS(ptr, count, 20);
    return ptr;
}

void operator delete(void* ptr) noexcept{
    TracyFreeS(ptr, 20);
    free(ptr);
}
#endif