//
// Created by jglrxavpok on 21/11/2020.
//

#define GLM_FORCE_RADIANS
#include <filesystem>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <core/utils/ImGuiUtils.hpp>

#include "engine/Engine.h"
#include <chrono>
#include <algorithm>
#include <stdexcept>
#include <vector>
#include <set>
#include <core/async/OSThreads.h>
#include <engine/render/shaders/ShaderStages.h>
#include "engine/constants.h"
#include "engine/render/resources/Image.h"
#include "engine/render/resources/SingleMesh.h"
#include "engine/render/resources/Vertex.h"
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

#include <core/scripting/csharp/Engine.h>
#include <core/scripting/csharp/CSAssembly.h>
#include <core/scripting/csharp/CSClass.h>
#include <core/scripting/csharp/CSMethod.h>
#include <core/scripting/csharp/CSObject.h>
#include <engine/scripting/CSharpBindings.h>
#include <engine/ecs/components/SoundListenerComponent.h>
#include <engine/ecs/systems/SoundListenerSystem.h>

#include "engine/io/actions/ActionDebug.h"
#include "engine/render/Sprite.h"
#include "engine/physics/PhysicsSystem.h"

#include "engine/ecs/systems/System.h"
#include "engine/ecs/systems/BillboardSystem.h"
#include "engine/ecs/systems/ModelRenderSystem.h"
#include "engine/ecs/systems/AnimatedModelRenderSystem.h"
#include "engine/ecs/systems/PhysicsCharacterSystem.h"
#include "engine/ecs/systems/RigidBodySystem.h"
#include "engine/ecs/systems/SpriteRenderSystem.h"
#include "engine/ecs/systems/SystemHandleLights.h"
#include "engine/ecs/systems/SystemKinematics.h"
#include "engine/ecs/systems/SystemSinPosition.h"
#include "engine/ecs/systems/SystemTransformSwapBuffers.h"
#include "engine/ecs/systems/CameraSystem.h"
#include "engine/ecs/systems/TextRenderSystem.h"

#include "engine/ecs/components/Component.h"
#include "engine/ecs/components/BillboardComponent.h"
#include "engine/ecs/components/CameraComponent.h"
#include "engine/ecs/components/ForceSinPosition.h"
#include "engine/ecs/components/Kinematics.h"
#include "engine/ecs/components/LuaScriptComponent.h"
#include "engine/ecs/components/LightComponent.h"
#include "engine/ecs/components/ModelComponent.h"
#include "engine/ecs/components/NavMeshComponent.h"
#include "engine/ecs/components/PhysicsCharacterComponent.h"
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

#if USE_LIVEPP
#include "LPP_API_x64_CPP.h"
static lpp::LppSynchronizedAgent lppAgent;
#endif

Carrot::Engine::SetterHack::SetterHack(Carrot::Engine* e) {
    Carrot::Engine::instance = e;
    Carrot::IO::Resource::vfsToUse = &e->vfs;
    auto exePath = Carrot::IO::getExecutablePath();
    e->vfs.addRoot("engine", exePath.parent_path());
    std::filesystem::current_path(exePath.parent_path());
}

Carrot::Engine::Engine(Configuration config): mainWindow(*this, WINDOW_WIDTH, WINDOW_HEIGHT, config),
instanceSetterHack(this),
vrInterface(config.runInVR ? std::make_unique<VR::Interface>(*this) : nullptr),
vkDriver(mainWindow, config, this, vrInterface.get()),
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

#if USE_LIVEPP
    std::filesystem::path livePPPath = std::filesystem::current_path() / "LivePP" / "";
    lppAgent = lpp::LppCreateSynchronizedAgent(livePPPath.wstring().c_str());

    // bail out in case the agent is not valid
    if (!lpp::LppIsValidSynchronizedAgent(&lppAgent))
    {
        throw std::invalid_argument("Could not initialize Live++.");
    }

    // enable Live++ for all loaded modules
    lppAgent.EnableModule(lpp::LppGetCurrentModulePath(), lpp::LPP_MODULES_OPTION_ALL_IMPORT_MODULES, nullptr, nullptr);
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

    createViewport(mainWindow); // main viewport
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

                                                                 renderer.recordImGuiPass(cmds, pass.getRenderPass(), frame);

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
        Render::GraphBuilder leftEyeGraph(vkDriver, mainWindow);
        Render::GraphBuilder mainGraph(vkDriver, mainWindow);
        Render::Composer companionComposer(vkDriver);

        auto leftEyeFinalPass = fillGraphBuilder(leftEyeGraph, Render::Eye::LeftEye);
        auto& rightEyeFinalPass = leftEyeFinalPass;

        Render::GraphBuilder rightEyeGraph = leftEyeGraph; // reuse most textures

        composers[Render::Eye::LeftEye]->add(leftEyeFinalPass);
        auto& leftEyeComposerPass = composers[Render::Eye::LeftEye]->appendPass(leftEyeGraph);

        composers[Render::Eye::RightEye]->add(rightEyeFinalPass);
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
        Render::GraphBuilder mainGraph(vkDriver, mainWindow);

        auto lastPass = fillGraphBuilder(mainGraph);

        composers[Render::Eye::NoVR]->add(lastPass);
        auto& composerPass = composers[Render::Eye::NoVR]->appendPass(mainGraph);
        addPresentPass(mainGraph, composerPass.getData().color);

        getMainViewport().setRenderGraph(std::move(mainGraph.compile()));
    }

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
            for(int axisID = 0; axisID <= GLFW_GAMEPAD_AXIS_LAST; axisID++) {
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
#if USE_LIVEPP
        if (lppAgent.WantsReload()) {
            lppAgent.CompileAndReloadChanges(lpp::LPP_RELOAD_BEHAVIOUR_WAIT_UNTIL_CHANGES_ARE_APPLIED);
        }
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

            if(config.enableFileWatching) {
                Carrot::Async::Counter watchSync;
                for(const auto& ref : fileWatchers) {
                    if(auto ptr = ref.lock()) {
                        taskScheduler.schedule(Carrot::TaskDescription {
                                .name = "File watching",
                                .task = [ptr](Carrot::TaskHandle& task) {
                                    ptr->tick();
                                },
                                .joiner = &watchSync,
                        }, Carrot::TaskScheduler::FrameParallelWork);
                    }
                }
                watchSync.busyWait();
            }
        }

        if(glfwWindowShouldClose(mainWindow.getGLFWPointer())) {
            if(game->onCloseButtonPressed()) {
                game->requestShutdown();
            } else {
                glfwSetWindowShouldClose(mainWindow.getGLFWPointer(), false);
            }
        }

        if(game->hasRequestedShutdown()) {
            running = false;
            break;
        }

        {
            ZoneScopedN("Poll inputs");
            for(auto& [id, callback] : mouseWheelCallbacks) {
                callback(0.0f); // reset mouse wheel
            }
            glfwPollEvents();
            pollKeysVec2();
            pollGamepads();
            if(config.runInVR) {
                IO::ActionSet::syncXRActions();
            }


            onMouseMove(mainWindow, mouseX, mouseY, true); // Reset input actions based mouse dx/dy
        }

        {
            ZoneScopedN("Setup frame");
            renderer.newFrame();

            {
                ZoneScopedN("nextFrameAwaiter.resume_all()");
                nextFrameAwaiter.resume_all();
            }
        }

        if(showInputDebug) {
            Carrot::IO::debugDrawActions();
        }

        auto tickStartTime = std::chrono::steady_clock::now();
        {
            ZoneScopedN("Tick");
            TracyPlot("Tick lag", lag.count());
            TracyPlot("Estimated FPS", currentFPS);
            TracyPlot("Estimated VRAM usage", static_cast<std::int64_t>(Carrot::DeviceMemory::TotalMemoryUsed.load()));
            TracyPlotConfig("Estimated VRAM usage", tracy::PlotFormatType::Memory);

            const std::uint32_t maxCatchupTicks = 10;
            std::uint32_t caughtUp = 0;
            while(lag >= timeBetweenUpdates && caughtUp++ < maxCatchupTicks) {
                GetTaskScheduler().executeMainLoop();
                tick(timeBetweenUpdates.count());
                lag -= timeBetweenUpdates;

                Carrot::IO::ActionSet::updatePrePollAllSets(*this);
                Carrot::IO::ActionSet::resetAllDeltas();
            }
        }
        auto tickTimeElapsed = std::chrono::steady_clock::now() - tickStartTime;

        // record frame info even if debug option is not active
        frameTimeHistory.push(timeElapsed.count());
        tickTimeHistory.push(std::chrono::duration<float>(tickTimeElapsed).count());

        drawFrame(currentFrame);

        Carrot::Log::flush();

        nextFrameAwaiter.cleanup();

        currentFrame = (currentFrame+1) % MAX_FRAMES_IN_FLIGHT;

        FrameMark;

        Carrot::Threads::reduceCPULoad();
    }

    glfwHideWindow(mainWindow.getGLFWPointer());

    WaitDeviceIdle();
}

void Carrot::Engine::stop() {
    running = false;
}

static void windowResize(GLFWwindow* window, int width, int height) {
    auto pWindow = reinterpret_cast<Carrot::Window*>(glfwGetWindowUserPointer(window));
    GetEngine().onWindowResize(*pWindow);
}

static void mouseMove(GLFWwindow* window, double xpos, double ypos) {
    auto pWindow = reinterpret_cast<Carrot::Window*>(glfwGetWindowUserPointer(window));
    GetEngine().onMouseMove(*pWindow, xpos, ypos, false);
}

static void mouseButton(GLFWwindow* window, int button, int action, int mods) {
    auto pWindow = reinterpret_cast<Carrot::Window*>(glfwGetWindowUserPointer(window));
    GetEngine().onMouseButton(*pWindow, button, action, mods);
}

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    auto pWindow = reinterpret_cast<Carrot::Window*>(glfwGetWindowUserPointer(window));
    GetEngine().onKeyEvent(*pWindow, key, scancode, action, mods);

    if(!GetEngine().isGrabbingCursor()) {
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
    auto pWindow = reinterpret_cast<Carrot::Window*>(glfwGetWindowUserPointer(window));
    GetEngine().onScroll(*pWindow, xScroll, yScroll);

    if(!GetEngine().isGrabbingCursor()) {
        ImGui_ImplGlfw_ScrollCallback(window, xScroll, yScroll);
    }
}

void Carrot::Engine::initWindow() {
    glfwSetJoystickCallback(joystickCallback);

    auto& window = mainWindow; // TODO: reuse for different windows
    glfwSetFramebufferSizeCallback(window.getGLFWPointer(), windowResize);

    glfwSetCursorPosCallback(window.getGLFWPointer(), mouseMove);
    glfwSetMouseButtonCallback(window.getGLFWPointer(), mouseButton);
    glfwSetKeyCallback(window.getGLFWPointer(), keyCallback);
    glfwSetScrollCallback(window.getGLFWPointer(), scrollCallback);
}

void Carrot::Engine::initVulkan() {
    renderer.lateInit();

    createCameras();

    initScripting();
    initECS();
    initGame();

    createSynchronizationObjects();
}

void Carrot::Engine::initScripting() {
    scriptingEngine = std::make_unique<Scripting::ScriptingEngine>();
    csBindings = std::make_unique<Scripting::CSharpBindings>();

    sceneManager.initScripting();
    audioManager.initScripting();
}

void Carrot::Engine::initECS() {
    auto& components = Carrot::ECS::getComponentLibrary();
    auto& systems = Carrot::ECS::getSystemLibrary();

    {
        components.add<Carrot::ECS::TransformComponent>();
        components.add<Carrot::ECS::Kinematics>();
        components.add<Carrot::ECS::SpriteComponent>();
        components.add<Carrot::ECS::ModelComponent>();
        components.add<Carrot::ECS::AnimatedModelComponent>();
        components.add<Carrot::ECS::ForceSinPosition>();
        components.add<Carrot::ECS::LightComponent>();
        components.add<Carrot::ECS::RigidBodyComponent>();
        components.add<Carrot::ECS::CameraComponent>();
        components.add<Carrot::ECS::TextComponent>();
        components.add<Carrot::ECS::LuaScriptComponent>();
        components.add<Carrot::ECS::PhysicsCharacterComponent>();
        components.add<Carrot::ECS::NavMeshComponent>();
        components.add<Carrot::ECS::SoundListenerComponent>();
        components.add<Carrot::ECS::BillboardComponent>();
    }

    {
        systems.addUniquePtrBased<Carrot::ECS::ModelRenderSystem>();
        systems.addUniquePtrBased<Carrot::ECS::AnimatedModelRenderSystem>();
        systems.addUniquePtrBased<Carrot::ECS::RigidBodySystem>();
        systems.addUniquePtrBased<Carrot::ECS::SpriteRenderSystem>();
        systems.addUniquePtrBased<Carrot::ECS::SystemHandleLights>();
        systems.addUniquePtrBased<Carrot::ECS::SystemKinematics>();
        systems.addUniquePtrBased<Carrot::ECS::SystemSinPosition>();
        systems.addUniquePtrBased<Carrot::ECS::CameraSystem>();
        systems.addUniquePtrBased<Carrot::ECS::TextRenderSystem>();
        systems.addUniquePtrBased<Carrot::ECS::SystemTransformSwapBuffers>();
        systems.addUniquePtrBased<Carrot::ECS::PhysicsCharacterSystem>();
        systems.addUniquePtrBased<Carrot::ECS::SoundListenerSystem>();
        systems.addUniquePtrBased<Carrot::ECS::BillboardSystem>();

        systems.addUniquePtrBased<Carrot::ECS::LuaRenderSystem>();
        systems.addUniquePtrBased<Carrot::ECS::LuaUpdateSystem>();
    }
}

Carrot::Engine::~Engine() {
    Carrot::Render::Sprite::cleanup();
    renderer.shutdownImGui();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    for(auto& ctx : tracyCtx) {
        TracyVkDestroy(ctx);
    }
    tracyCtx.clear();

#if USE_LIVEPP
    lpp::LppDestroySynchronizedAgent(&lppAgent);
#endif
/*    for(size_t i = 0; i < getSwapchainImageCount(); i++) {
        TracyVkDestroy(tracyCtx[i]);
    }*/
}

std::unique_ptr<Carrot::CarrotGame>& Carrot::Engine::getGame() {
    return game;
}

// TODO: move to renderer
void Carrot::Engine::recordMainCommandBufferAndPresent(std::uint8_t _frameIndex, const Render::Context& mainRenderContext) {
    ZoneScopedN("Record main command buffer");
    const std::uint32_t frameIndex = _frameIndex; // index of image in frames in flight, 0 or 1 currently (MAX_FRAMES_IN_FLIGHT)
    const std::uint32_t swapchainIndex = mainRenderContext.swapchainIndex; // index of image in swapchain, on my machine, 0 to 2 (inclusive)
    // main command buffer
    vk::CommandBufferBeginInfo beginInfo{
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
            .pInheritanceInfo = nullptr,
    };

    {
        {
            ZoneScopedN("mainCommandBuffers[frameIndex].begin(beginInfo)");
            mainCommandBuffers[frameIndex].begin(beginInfo);

            DebugNameable::nameSingle(Carrot::sprintf("Main command buffer, frame %lu", renderer.getFrameCount()), mainCommandBuffers[frameIndex]);
            GetVulkanDriver().setMarker(mainCommandBuffers[frameIndex], "begin command buffer");
        }

        TracyVkZone(tracyCtx[swapchainIndex], mainCommandBuffers[frameIndex], "Main command buffer");

        if(config.runInVR) {
            {
                ZoneScopedN("VR Left eye render");
                GetVulkanDriver().setMarker(mainCommandBuffers[frameIndex], "VR Left eye render");
                leftEyeGlobalFrameGraph->execute(newRenderContext(swapchainIndex, getMainViewport(), Render::Eye::LeftEye), mainCommandBuffers[frameIndex]);
            }
            {
                ZoneScopedN("VR Right eye render");
                GetVulkanDriver().setMarker(mainCommandBuffers[frameIndex], "VR Right eye render");
                rightEyeGlobalFrameGraph->execute(newRenderContext(swapchainIndex, getMainViewport(), Render::Eye::RightEye), mainCommandBuffers[frameIndex]);
            }
        }

        {
            GetVulkanDriver().setMarker(mainCommandBuffers[frameIndex], "render viewports");
            ZoneScopedN("Render viewports");
            for(auto it = viewports.rbegin(); it != viewports.rend(); it++) {
                ZoneScopedN("Render single viewport");
                auto& viewport = *it;
                GetVulkanDriver().setFormattedMarker(mainCommandBuffers[frameIndex], "render viewport %x", &viewport);
                if(config.runInVR) {
                    // each eye is done in separate render passes above this code
                    viewport.render(newRenderContext(swapchainIndex, viewport, Render::Eye::NoVR), mainCommandBuffers[frameIndex]);
                } else {
                    viewport.render(newRenderContext(swapchainIndex, viewport, Render::Eye::NoVR), mainCommandBuffers[frameIndex]);
                }
            }
        }

        TracyVkCollect(tracyCtx[swapchainIndex], mainCommandBuffers[frameIndex]);
    }

    mainCommandBuffers[frameIndex].end();

    {
        ZoneScopedN("Present");

        std::vector<vk::Semaphore> waitSemaphores;
        std::vector<vk::PipelineStageFlags> waitStages;
        waitSemaphores.reserve(1 + externalWindows.size());
        waitStages.reserve(1 + externalWindows.size());
        waitSemaphores.emplace_back(mainWindow.getImageAvailableSemaphore(frameIndex));
        waitStages.emplace_back(vk::PipelineStageFlagBits::eColorAttachmentOutput);

        for(auto& window : externalWindows) {
            if(!window.hasAcquiredAtLeastOneImage()) {
                continue;
            }
            if(window.isFirstPresent()) {
                continue;
            }

            waitSemaphores.emplace_back(window.getImageAvailableSemaphore(frameIndex));
            waitStages.emplace_back(vk::PipelineStageFlagBits::eColorAttachmentOutput);
        }

        vk::Semaphore signalSemaphores[] = {*renderFinishedSemaphore[frameIndex]};

        game->changeGraphicsWaitSemaphores(frameIndex, waitSemaphores, waitStages);

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
                .pCommandBuffers = &mainCommandBuffers[frameIndex],

                .signalSemaphoreCount = 1,
                .pSignalSemaphores = signalSemaphores,
        };

        {
            ZoneScopedN("Renderer Pre-Frame actions");
            renderer.preFrame(mainRenderContext);
        }

        {
            ZoneScopedN("Reset in flight fences");
            getLogicalDevice().resetFences(*inFlightFences[frameIndex]);
        }
        {
            ZoneScopedN("Submit to graphics queue");
            //presentThread.present(currentFrame, signalSemaphores[0], submitInfo, *inFlightFences[currentFrame]);

            waitForFrameTasks();

            GetVulkanDriver().submitGraphics(submitInfo, *inFlightFences[frameIndex]);

            std::vector<vk::SwapchainKHR> swapchains;
            std::vector<std::uint32_t> swapchainIndices;
            swapchains.reserve(externalWindows.size() + 1);
            swapchainIndices.reserve(externalWindows.size() + 1);
            swapchains.emplace_back(mainWindow.getSwapchain());
            swapchainIndices.emplace_back(swapchainIndex);

            std::size_t windowIndex = 1;
            for(auto& window : externalWindows) {
                if(!window.hasAcquiredAtLeastOneImage()) {
                    continue;
                }
                if(window.isFirstPresent()) {
                    // because we are recording the previous frame (compared to the one being updated/ticked/drawn),
                    //  there is a frame where we create a new window, but its swapchain image was not acquired.
                    // In that case, don't attempt to present that swapchain image
                    window.clearFirstPresent();
                    continue;
                }
                swapchains.emplace_back(window.getSwapchain());
                swapchainIndices.emplace_back((std::uint32_t)((swapchainIndex + window.getSwapchainIndexOffset()) % getSwapchainImageCount()));
            }

            vk::PresentInfoKHR presentInfo{
                    .waitSemaphoreCount = 1,
                    .pWaitSemaphores = signalSemaphores,

                    .swapchainCount = static_cast<std::uint32_t>(swapchains.size()),
                    .pSwapchains = swapchains.data(),
                    .pImageIndices = swapchainIndices.data(),
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
                vrSession->present(mainRenderContext);
            }
        }

        {
            ZoneScopedN("Renderer Post-Frame actions");
            renderer.postFrame();
        }
    }
}

void Carrot::Engine::allocateGraphicsCommandBuffers() {
    vk::CommandBufferAllocateInfo allocInfo{
            .commandPool = getGraphicsCommandPool(),
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = static_cast<uint32_t>(getSwapchainImageCount()),
    };

    this->mainCommandBuffers = this->getLogicalDevice().allocateCommandBuffers(allocInfo);
}

void Carrot::Engine::drawFrame(size_t currentFrame) {
    ZoneScoped;

    lastFrameIndex = swapchainImageIndexRightNow;

    auto cancelFrame = []() {
        ImGui::EndFrame();
        ImGui::UpdatePlatformWindows();
    };
    if(framebufferResized) {
        recreateSwapchain(mainWindow);
        cancelFrame();
        return;
    }
    frames++;

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

        auto acquire = [&](Window& window) -> std::int32_t {
            ZoneScopedN("acquireNextImageKHR");
            auto nextImage = getLogicalDevice().acquireNextImageKHR(window.getSwapchain(), UINT64_MAX,
                                                                    window.getImageAvailableSemaphore(currentFrame), nullptr);
            result = nextImage.result;

            if(result == vk::Result::eNotReady) {
                Carrot::Log::error("Had to cancel rendering for this frame, acquireNextImageKHR returned not ready");
                //return -1;
            } else if (result == vk::Result::eErrorOutOfDateKHR) {
                recreateSwapchain(window);
                return -1;
            } else if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
                throw std::runtime_error("Failed to acquire swap chain image");
            }
            return nextImage.value;
        };

        std::int32_t acquireResult = acquire(mainWindow);

        if(acquireResult == -1) {
            cancelFrame();
            return;
        }

        imageIndex = acquireResult;
        swapchainImageIndexRightNow = imageIndex;

        for(auto& window : externalWindows) {
            acquireResult = acquire(window);
            if(acquireResult == -1) {
                cancelFrame();
                return;
            }
            window.setCurrentSwapchainIndex(swapchainImageIndexRightNow, acquireResult);
        }
    }

    Carrot::Render::Context mainRenderContext = newRenderContext(imageIndex, getMainViewport());
    vkDriver.newFrame(mainRenderContext);

    {
        ZoneScopedN("Prepare frame");
        auto onFrameTimeStart = std::chrono::steady_clock::now();

        if(config.runInVR) {
            ZoneScopedN("VR start frame");
            vrSession->startFrame();
        }

        vkDriver.startFrame(mainRenderContext);
        assetServer.beginFrame(mainRenderContext);
        resourceAllocator->beginFrame(mainRenderContext);
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
                    v.getCamera().updateFrustum();
                    game->onFrame(newRenderContext(imageIndex, v));
                } else {
                    game->setupCamera(renderContext);
                    v.getCamera().updateFrustum();
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

        if(showFPS) {
            bool bOpen = true;
            if(ImGui::Begin("FPS Counter", &bOpen, ImGuiWindowFlags_::ImGuiWindowFlags_NoCollapse)) {
                ImGui::Text("Estimated FPS: %.3f", currentFPS);

                const float expectedDelta = 1.0f / (float)config.tickRate;
                const float goodDelta = 1.0f / (2.0f * config.tickRate);
                const float mehDelta = 1.0f / config.tickRate * 2.0f;
                const float badDelta = 1.0f / config.tickRate * 4.0f;
                const std::vector<float> timestamps {
                        0.0f,
                        goodDelta,
                        expectedDelta,
                        mehDelta,
                        badDelta,
                };

                const std::vector<glm::vec3> colors {
                        glm::vec3{0, 0, 1}, // woah
                        glm::vec3{0, 0.2, 1}, // good
                        glm::vec3{0, 1, 0}, // expected
                        glm::vec3{1, 1, 0}, // meh
                        glm::vec3{1, 0, 0}, // bad
                };

                auto timeToColor = [&](float time) {
                    auto toU32 = [](const glm::vec3& color) {
                        return 0xFF000000
                               | ((int)(color.b * 255) & 0xFF) << 16
                               | ((int)(color.g * 255) & 0xFF) <<  8
                               | ((int)(color.r * 255) & 0xFF) <<  0
                                ;
                    };

                    for(int i = 1; i < timestamps.size(); i++) {
                        if(time < timestamps[i]) {
                            float t = (time - timestamps[i-1]) / (timestamps[i] - timestamps[i-1]);
                            return toU32((1-t) * colors[i-1] + t * colors[i]);
                        }
                    }
                    return toU32(colors.back());
                };

                auto drawHistory = [&](CircleBuffer<float, 600>& history, float maxHeight, float minimumTime) {
                    if(history.getCount() > 0) {
                        ImGui::Text("%.3f ms", history[history.getCurrentPosition()-1]*1000.0f);
                    }

                    // based on blog https://asawicki.info/news?x=view&year=2022&month=5
                    ImVec2 availableSpace = ImGui::GetContentRegionAvail();
                    availableSpace.y = maxHeight;
                    const ImVec2 cursorPos = ImGui::GetCursorScreenPos();
                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    drawList->AddRectFilled(cursorPos, cursorPos + availableSpace, 0x020202FF);

                    float rightmostX = cursorPos.x + availableSpace.x;
                    auto drawBar = [&](float value) {
                        const float frameWidthScale = goodDelta;
                        const float frameWidth = value / frameWidthScale;
                        const float frameHeightScale = availableSpace.y / (std::log2f(badDelta) - std::log2f(minimumTime));
                        float frameHeight = (std::log2f(value) - std::log2f(minimumTime)) * frameHeightScale;
                        if(frameHeight < 5.0f) {
                            frameHeight = 5.0f;
                        } else if(frameHeight >= availableSpace.y) {
                            frameHeight = availableSpace.y;
                        }
                        const std::uint32_t frameColor = timeToColor(value);

                        drawList->AddRectFilled(ImVec2(rightmostX - frameWidth, cursorPos.y - frameHeight + availableSpace.y),
                                                ImVec2(rightmostX, cursorPos.y + availableSpace.y),
                                                frameColor);
                        rightmostX -= frameWidth;
                        rightmostX = floor(rightmostX);
                    };

                    if(history.getCount() < history.MaxCount) {
                        for(int i = history.getCount() - 1; i >= 0; i--) {
                            drawBar(history[i]);
                        }
                    } else {
                        for(int i = history.getCurrentPosition() - 1; i >= history.getCurrentPosition() - history.MaxCount; i--) {
                            int index = i;
                            if(index < 0) {
                                index += history.MaxCount;
                            }
                            drawBar(history[index]);
                        }
                    }
                    ImGui::Dummy(availableSpace);
                };
                ImGui::Text("Total frame time");
                drawHistory(frameTimeHistory, 50, goodDelta);

                ImGui::Text("Tick time");
                drawHistory(tickTimeHistory, 50, expectedDelta);

                ImGui::Text("OnFrame time");
                drawHistory(onFrameTimeHistory, 50, expectedDelta);

                ImGui::Text("Record time");
                drawHistory(recordTimeHistory, 50, goodDelta);
            }
            ImGui::End();

            if(!bOpen) {
                showFPS.setValue(false);
            }
        }

        assetServer.beforeRecord(mainRenderContext);
        renderer.startRecord(currentFrame, mainRenderContext);

        auto onFrameTimeElapsed = std::chrono::steady_clock::now() - onFrameTimeStart;
        onFrameTimeHistory.push(std::chrono::duration<float>(onFrameTimeElapsed).count());
        recordTimeHistory.push(renderer.getLastRecordDuration());
    }
}

void Carrot::Engine::createSynchronizationObjects() {
    renderFinishedSemaphore.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    vk::SemaphoreCreateInfo semaphoreInfo{};

    vk::FenceCreateInfo fenceInfo{
            .flags = vk::FenceCreateFlagBits::eSignaled,
    };

    for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        renderFinishedSemaphore[i] = getLogicalDevice().createSemaphoreUnique(semaphoreInfo, vkDriver.getAllocationCallbacks());
        inFlightFences[i] = getLogicalDevice().createFenceUnique(fenceInfo, vkDriver.getAllocationCallbacks());
    }
}

void Carrot::Engine::recreateSwapchain(Window& window) {
    Carrot::Log::info("recreateSwapchain");
    window.fetchNewFramebufferSize(); // TODO: how to handle minimized windows?

    renderer.waitForRenderToComplete();

    framebufferResized = false;

    WaitDeviceIdle();

    std::size_t previousImageCount = getSwapchainImageCount();
    window.cleanupSwapChain();
    window.createSwapChain();

    // TODO: only recreate if necessary
    if(previousImageCount != vkDriver.getSwapchainImageCount()) {
        onSwapchainImageCountChange(vkDriver.getSwapchainImageCount());
    }
    vk::Extent2D swapchainImageSize = vkDriver.getFinalRenderSize(window);
    onSwapchainSizeChange(window, swapchainImageSize.width, swapchainImageSize.height);

    if(!window.isMainWindow()) {
        window.computeSwapchainOffset(); // reset swapchain index
    }
}

void Carrot::Engine::onWindowResize(Window& which) {
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
        auto camera = Camera(45.0f, mainWindow.getFramebufferExtent().width / (float) mainWindow.getFramebufferExtent().height, 0.1f, 1000.0f);
        camera.getPositionRef() = glm::vec3(center.x, center.y + 1, 5.0f);
        camera.getTargetRef() = center;
        getMainViewport().getCamera(Render::Eye::NoVR) = std::move(camera);
    }
}

void Carrot::Engine::onMouseMove(Window& which, double xpos, double ypos, bool updateOnlyDelta) {
    if(which != mainWindow) {
        return;
    }
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

void Carrot::Engine::onMouseButton(Window& which, int button, int action, int mods) {
    if(which != mainWindow) {
        return;
    }
    for(auto& [id, callback] : mouseButtonCallbacks) {
        callback(button, action == GLFW_PRESS || action == GLFW_REPEAT, mods);
    }
}

void Carrot::Engine::onKeyEvent(Window& which, int key, int scancode, int action, int mods) {
    if(key == GLFW_KEY_GRAVE_ACCENT && action == GLFW_RELEASE) {
        Console::instance().toggleVisibility();
    }

    if(key == GLFW_KEY_F2 && action == GLFW_PRESS) {
        takeScreenshot();
    }

    if(which != mainWindow) {
        return;
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
}

void Carrot::Engine::onScroll(Window& which, double xScroll, double yScroll) {
    if(which != mainWindow) {
        return;
    }
    // TODO: transfer xScroll?
    for(auto& [id, callback] : mouseWheelCallbacks) {
        callback(yScroll);
    }
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
        const std::string name = Carrot::sprintf("Main swapchainIndex %d", i);
        TracyVkContextName(tracyCtx[i], name.c_str(), name.size());
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
    currentTime += deltaTime;
    {
        ZoneScopedN("Game tick");
        game->tick(deltaTime);
    }
    audioManager.tick(deltaTime);
    assetServer.tick(deltaTime);

    auto prePhysics = [&]() {
        game->prePhysics();
    };
    auto postPhysics = [&]() {
        game->postPhysics();
    };
    GetPhysics().tick(deltaTime, prePhysics, postPhysics);
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

    std::shared_ptr<Render::Texture> lastImage = mainWindow.getSwapchainTexture(lastFrameIndex);

    auto& swapchainExtent = vkDriver.getFinalRenderSize(mainWindow);
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


std::shared_ptr<Carrot::Render::Texture> Carrot::Engine::getSkyboxCubeMap() const {
    return loadedSkyboxTexture;
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
}

void Carrot::Engine::onSwapchainSizeChange(Window& window, int newWidth, int newHeight) {
    if(config.runInVR && window.isMainWindow()) {
        leftEyeGlobalFrameGraph->onSwapchainSizeChange(window, newWidth, newHeight);
        rightEyeGlobalFrameGraph->onSwapchainSizeChange(window, newWidth, newHeight);
    }
    getMainViewport().onSwapchainSizeChange(window, newWidth, newHeight);
    for(auto& v : viewports) {
        if(&v == &getMainViewport()) {
            continue;
        }
        v.onSwapchainSizeChange(window, newWidth, newHeight);
    }

    game->onSwapchainSizeChange(window, newWidth, newHeight);
}

Carrot::Render::Context Carrot::Engine::newRenderContext(std::size_t swapchainFrameIndex, Carrot::Render::Viewport& viewport, Carrot::Render::Eye eye) {
    return Carrot::Render::Context {
            .renderer = renderer,
            .pViewport = &viewport,
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

Carrot::Render::Viewport& Carrot::Engine::createViewport(Window& window) {
    verify(viewports.size() < VulkanRenderer::MaxViewports, "Too many viewports!");
    viewports.emplace_back(renderer, window.getWindowID());
    return viewports.back();
}

void Carrot::Engine::destroyViewport(Carrot::Render::Viewport& viewport) {
    // ensure viewport is not used while we delete it
    renderer.waitForRenderToComplete();
    WaitDeviceIdle();
    viewports.remove_if([&](const Carrot::Render::Viewport& v) {
        return &v == &viewport;
    });
}

Carrot::Window& Carrot::Engine::getMainWindow() {
    return mainWindow;
}

Carrot::Window& Carrot::Engine::getWindow(WindowID id) {
    if(mainWindow == id) {
        return mainWindow;
    }

    for(auto& window : externalWindows) {
        if(window == id) {
            return window;
        }
    }

    verify(false, Carrot::sprintf("No matching window with ID %ll !", id));
    return *((Carrot::Window*)nullptr);
}

Carrot::Window& Carrot::Engine::createWindow(const std::string& title, std::uint32_t w, std::uint32_t h) {
    externalWindows.emplace_back(*this, w, h, config);
    auto& window = externalWindows.back();
    window.setTitle(title);
    window.computeSwapchainOffset();
    return window;
}

Carrot::Window& Carrot::Engine::createAdoptedWindow(vk::SurfaceKHR surface) {
    externalWindows.emplace_back(*this, surface);
    auto& window = externalWindows.back();
    window.computeSwapchainOffset();
    return window;
}

void Carrot::Engine::destroyWindow(Carrot::Window& window) {
    externalWindows.remove_if([&](const Carrot::Window& windowInList) {
        return windowInList == window;
    });
}

Carrot::Audio::AudioManager& Carrot::Engine::getAudioManager() {
    return audioManager;
}

Carrot::SceneManager& Carrot::Engine::getSceneManager() {
    return sceneManager;
}

Carrot::AssetServer& Carrot::Engine::getAssetServer() {
    return assetServer;
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
    glfwSetInputMode(mainWindow.getGLFWPointer(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
}

void Carrot::Engine::ungrabCursor() {
    grabbingCursor = false;
    glfwSetInputMode(mainWindow.getGLFWPointer(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
}

void Carrot::Engine::changeTickRate(std::uint32_t tickRate) {
    timeBetweenUpdates = std::chrono::duration<float>(1.0f / tickRate);
}

double Carrot::Engine::getCurrentFrameTime() const {
    return currentTime;
}

Carrot::Scripting::ScriptingEngine& Carrot::Engine::getCSScriptEngine() {
    return *scriptingEngine;
}

Carrot::Scripting::CSharpBindings& Carrot::Engine::getCSBindings() {
    return *csBindings;
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