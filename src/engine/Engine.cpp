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
#include <engine/io/IO.h>
#include <engine/render/CameraBufferObject.h>
#include <engine/render/shaders/ShaderStages.h>
#include "engine/constants.h"
#include "engine/render/resources/Buffer.h"
#include "engine/render/resources/Image.h"
#include "engine/render/resources/Mesh.h"
#include "engine/render/Model.h"
#include "engine/render/Material.h"
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
#include "engine/utils/Macros.h"
#include "engine/io/actions/ActionSet.h"
#include "engine/io/Logging.hpp"
#include "engine/io/actions/ActionDebug.h"

#ifdef ENABLE_VR
#include "vr/VRInterface.h"
#endif

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

static Carrot::RuntimeOption showFPS("Debug/Show FPS", true);
static Carrot::RuntimeOption showInputDebug("Debug/Show Inputs", false);
static Carrot::RuntimeOption showGBuffer("Debug/Show GBuffer", false);


static std::unordered_set<int> activeJoysticks{};

Carrot::Engine::Engine(NakedPtr<GLFWwindow> window, Configuration config): window(window),
#ifdef ENABLE_VR
vrInterface(std::make_unique<VR::Interface>(*this)),
#endif
vkDriver(window, config, this
#ifdef ENABLE_VR
    , *vrInterface
#endif
),
renderer(vkDriver, config), screenQuad(std::make_unique<Mesh>(vkDriver, vector<ScreenSpaceVertex>{
                                                                                                                                   { { -1, -1} },
                                                                                                                                   { { 1, -1} },
                                                                                                                                   { { 1, 1} },
                                                                                                                                   { { -1, 1} },
                                                                                                                           },
                                                                                                                           vector<uint32_t>{
                                                                                                                                   2,1,0,
                                                                                                                                   3,2,0,
                                                                                                                           })),
    presentThread(vkDriver),
    config(config)
    {
    ZoneScoped;
#ifndef ENABLE_VR
    if(config.runInVR) {
        //Carrot::crash("");
        throw std::runtime_error("Tried to launch engine in VR, but ENABLE_VR was not defined during compilation.");
    }
#else
    vrSession = vrInterface->createSession();
    vkDriver.getTextureRepository().setXRSession(vrSession.get());
#endif

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

    // quickly render something on screen
    LoadingScreen screen{*this};
    initVulkan();

    struct PresentPassData {
        Render::FrameResource input;
        Render::FrameResource output;
    };

    auto& testTexture = renderer.getOrCreateTexture("default.png");

    auto fullscreenBlit = [this](const vk::RenderPass& pass, const Carrot::Render::Context& frame, Carrot::Render::Texture& textureToBlit, Carrot::Render::Texture& targetTexture, vk::CommandBuffer& cmds) {
        auto pipeline = renderer.getOrCreateRenderPassSpecificPipeline("blit", pass);
        vk::DescriptorImageInfo imageInfo {
                .sampler = vkDriver.getLinearSampler(),
                .imageView = textureToBlit.getView(),
                .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
        };

        auto& set = pipeline->getDescriptorSets0()[frame.swapchainIndex];
        vk::WriteDescriptorSet writeImage {
                .dstSet = set,
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eSampledImage,
                .pImageInfo = &imageInfo,
        };
        vkDriver.getLogicalDevice().updateDescriptorSets(writeImage, {});

        pipeline->bind(pass, frame, cmds);
        screenQuad->bind(cmds);
        screenQuad->draw(cmds);
    };

    auto fillGraphBuilder = [&](Render::GraphBuilder& mainGraph, bool shouldPresentToSwapchain, Render::Eye eye = Render::Eye::NoVR) {
        auto& rtPass = getRayTracer().appendRTPass(mainGraph, eye);

        auto& skyboxPass = mainGraph.addPass<Carrot::Render::PassData::Skybox>("skybox",
           [this](Render::GraphBuilder& builder, Render::Pass<Carrot::Render::PassData::Skybox>& pass, Carrot::Render::PassData::Skybox& data) {
               data.output = builder.createRenderTarget(vk::Format::eR8G8B8A8Unorm,
                                                        {},
                                                        vk::AttachmentLoadOp::eClear,
                                                        vk::ClearColorValue(std::array{0,0,0,1})
               );
           },
           [this](const Render::CompiledPass& pass, const Render::Context& frame, const Carrot::Render::PassData::Skybox& data, vk::CommandBuffer& buffer) {
                ZoneScopedN("CPU RenderGraph skybox");
                auto skyboxPipeline = renderer.getOrCreateRenderPassSpecificPipeline("skybox", pass.getRenderPass());
                renderer.bindCameraSet(vk::PipelineBindPoint::eGraphics, skyboxPipeline->getPipelineLayout(), frame, buffer);
                renderer.bindTexture(*skyboxPipeline, frame, *loadedSkyboxTexture, 0, 0, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::eCube);
                skyboxPipeline->bind(pass.getRenderPass(), frame, buffer);
                skyboxMesh->bind(buffer);
                skyboxMesh->draw(buffer);
           }
        );
        skyboxPass.setCondition([this](const Render::CompiledPass& pass, const Render::Context& frame, const Carrot::Render::PassData::Skybox& data) {
            return currentSkybox != Skybox::Type::None;
        });

        auto& gbufferPass = getGBuffer().addGBufferPass(mainGraph, [&](const Render::CompiledPass& pass, const Render::Context& frame, vk::CommandBuffer& cmds) {
            ZoneScopedN("CPU RenderGraph GPass");
            game->recordGBufferPass(pass.getRenderPass(), frame, cmds);
        });
        auto& gresolvePass = getGBuffer().addGResolvePass(gbufferPass.getData(), rtPass.getData(), skyboxPass.getData().output, mainGraph);

        gResolvePassData = gresolvePass.getData();

        composers[eye]->add(gresolvePass.getData().resolved);
        auto& composerPass = composers[eye]->appendPass(mainGraph);

        if(shouldPresentToSwapchain) {
//            auto& imguiPass = renderer.addImGuiPass(mainGraph, composerPass.getData().color);

            mainGraph.addPass<PresentPassData>("present",

                                               [prevPassData = composerPass.getData()](Render::GraphBuilder& builder, Render::Pass<PresentPassData>& pass, PresentPassData& data) {
                                                   // pass.rasterized = false;
                                                    data.input = builder.read(prevPassData.color, vk::ImageLayout::eShaderReadOnlyOptimal);
                                                    data.output = builder.write(builder.getSwapchainImage(), vk::AttachmentLoadOp::eClear, vk::ImageLayout::eColorAttachmentOptimal, vk::ClearColorValue(std::array{0,0,0,0}));
                                                    // uses ImGui, so no pre-record: pass.prerecordable = false;
                                                    builder.present(data.output);
                                               },
                                               [fullscreenBlit](const Render::CompiledPass& pass, const Render::Context& frame, const PresentPassData& data, vk::CommandBuffer& cmds) {
                                                   ZoneScopedN("CPU RenderGraph present");
                                                   auto& inputTexture = pass.getGraph().getTexture(data.input, frame.swapchainIndex);
                                                   auto& swapchainTexture = pass.getGraph().getTexture(data.output, frame.swapchainIndex);
                                                   fullscreenBlit(pass.getRenderPass(), frame, inputTexture, swapchainTexture, cmds);
                                                   ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmds);
                                                   //swapchainTexture.assumeLayout(vk::ImageLayout::eUndefined);
                                                   //frame.renderer.blit(inputTexture, swapchainTexture, cmds);
                                                   //swapchainTexture.transitionInline(cmds, vk::ImageLayout::ePresentSrcKHR);
                                               },
                                               [this](Render::CompiledPass& pass, PresentPassData& data) {
                                                   renderer.initImGuiPass(pass.getRenderPass());
                                               }
            );
        }
        return composerPass;
    };

    if(config.runInVR) {
        Render::GraphBuilder leftEyeGraph(vkDriver);
        Render::GraphBuilder rightEyeGraph(vkDriver);
        Render::GraphBuilder mainGraph(vkDriver);
        Render::Composer companionComposer(vkDriver);

        auto leftEyeFinalPass = fillGraphBuilder(leftEyeGraph, false, Render::Eye::LeftEye);
        auto rightEyeFinalPass = fillGraphBuilder(rightEyeGraph, false, Render::Eye::RightEye);

        companionComposer.add(leftEyeFinalPass.getData().color, -1.0, 0.0);
        companionComposer.add(rightEyeFinalPass.getData().color, 0.0, 1.0);

#ifdef ENABLE_VR
        vrSession->setEyeTexturesToPresent(leftEyeFinalPass.getData().color, rightEyeFinalPass.getData().color);
#endif

        auto& composerPass = companionComposer.appendPass(mainGraph);

        leftEyeGlobalFrameGraph = std::move(leftEyeGraph.compile());
        rightEyeGlobalFrameGraph = std::move(rightEyeGraph.compile());

       // auto& imguiPass = renderer.addImGuiPass(mainGraph);

        mainGraph.addPass<PresentPassData>("present",
                                           [prevPassData = composerPass.getData()](Render::GraphBuilder& builder, Render::Pass<PresentPassData>& pass, PresentPassData& data) {
                                               //pass.rasterized = false;
                                               data.input = builder.read(prevPassData.color, vk::ImageLayout::eShaderReadOnlyOptimal);
                                               data.output = builder.write(builder.getSwapchainImage(), vk::AttachmentLoadOp::eClear, vk::ImageLayout::eColorAttachmentOptimal, vk::ClearColorValue(std::array{0,0,0,0}));
                                               // uses ImGui, so no pre-record: pass.prerecordable = false;
                                               builder.present(data.output);
                                           },
                                           [fullscreenBlit](const Render::CompiledPass& pass, const Render::Context& frame, const PresentPassData& data, vk::CommandBuffer& cmds) {
                                               ZoneScopedN("CPU RenderGraph present");
                                               auto& inputTexture = pass.getGraph().getTexture(data.input, frame.swapchainIndex);
                                               auto& swapchainTexture = pass.getGraph().getTexture(data.output, frame.swapchainIndex);
                                               fullscreenBlit(pass.getRenderPass(), frame, inputTexture, swapchainTexture, cmds);
                                               ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmds);
                                               //swapchainTexture.assumeLayout(vk::ImageLayout::eUndefined);
                                               //frame.renderer.blit(inputTexture, swapchainTexture, cmds);
                                               //swapchainTexture.transitionInline(cmds, vk::ImageLayout::ePresentSrcKHR);
                                           },
                                           [this](Render::CompiledPass& pass, PresentPassData& data) {
                                               renderer.initImGuiPass(pass.getRenderPass());
                                           }
        );

        globalFrameGraph = std::move(mainGraph.compile());
    } else {
        Render::GraphBuilder mainGraph(vkDriver);

        fillGraphBuilder(mainGraph, true);

        globalFrameGraph = std::move(mainGraph.compile());
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
    for(auto& callback : gamepadButtonCallbacks) {
        callback(gamepadID, buttonID, pressed);
    }
}

void Carrot::Engine::onGamepadAxisChange(int gamepadID, int axisID, float newValue, float oldValue) {
    for(auto& callback : gamepadAxisCallbacks) {
        callback(gamepadID, axisID, newValue, oldValue);
    }
}

void Carrot::Engine::onGamepadVec2Change(int gamepadID, Carrot::IO::GameInputVectorType vecID, glm::vec2 newValue, glm::vec2 oldValue) {
    for(auto& callback : gamepadVec2Callbacks) {
        callback(gamepadID, vecID, newValue, oldValue);
    }
}

void Carrot::Engine::onKeysVec2Change(Carrot::IO::GameInputVectorType vecID, glm::vec2 newValue, glm::vec2 oldValue) {
    for(auto& callback : keysVec2Callbacks) {
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


    auto previous = chrono::steady_clock::now();
    auto lag = chrono::duration<float>(0.0f);
    const auto timeBetweenUpdates = chrono::duration<float>(1.0f/60.0f); // 60 Hz
    bool ticked = false;
    while(running) {
        auto frameStartTime = chrono::steady_clock::now();
        chrono::duration<float> timeElapsed = frameStartTime-previous;
        currentFPS = 1.0f / timeElapsed.count();
        lag += timeElapsed;
        previous = frameStartTime;

        // Reset input actions based mouse dx/dy
        onMouseMove(mouseX, mouseY, true);
        Carrot::IO::ActionSet::updatePrePollAllSets(*this, ticked);
        glfwPollEvents();
        pollKeysVec2();
        pollGamepads();
#ifdef ENABLE_VR
        if(config.runInVR) {
            ZoneScopedN("VR poll events");
            vrInterface->pollEvents();
        }
#endif
        if(glfwWindowShouldClose(window.get())) {
            running = false;
        }

        renderer.newFrame();
        ImGui::NewFrame();

        if(showInputDebug) {
            Carrot::IO::debugDrawActions();
        }

        {
            ZoneScopedN("Tick");
            TracyPlot("Tick lag", lag.count());
            TracyPlot("Estimated FPS", currentFPS);
            ticked = false;
            while(lag >= timeBetweenUpdates) {
                ticked = true;
                tick(timeBetweenUpdates.count());
                lag -= timeBetweenUpdates;
            }
        }

        if(showFPS) {
            if(ImGui::Begin("FPS Counter", nullptr, ImGuiWindowFlags_::ImGuiWindowFlags_NoCollapse)) {
                ImGui::Text("%f FPS", currentFPS);
            }
            ImGui::End();
        }

        drawFrame(currentFrame);

        currentFrame = (currentFrame+1) % MAX_FRAMES_IN_FLIGHT;

        FrameMark;
    }

    glfwHideWindow(window.get());

    getLogicalDevice().waitIdle();
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

    ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
}

static void joystickCallback(int joystickID, int event) {
    if(event == GLFW_CONNECTED) {
        activeJoysticks.insert(joystickID);
    } else if(event == GLFW_DISCONNECTED) {
        activeJoysticks.erase(joystickID);
    }
}

void Carrot::Engine::initWindow() {
    glfwSetWindowUserPointer(window.get(), this);
    glfwSetFramebufferSizeCallback(window.get(), windowResize);

    glfwSetCursorPosCallback(window.get(), mouseMove);
    glfwSetMouseButtonCallback(window.get(), mouseButton);
    glfwSetKeyCallback(window.get(), keyCallback);
    glfwSetJoystickCallback(joystickCallback);
}

void Carrot::Engine::initVulkan() {
    resourceAllocator = make_unique<ResourceAllocator>(vkDriver);

    createCameras();

    getRayTracer().init();

    initGame();

    getRayTracer().finishInit();

    createSynchronizationObjects();
}

Carrot::Engine::~Engine() {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    tracyCtx.clear();
/*    for(size_t i = 0; i < getSwapchainImageCount(); i++) {
        TracyVkDestroy(tracyCtx[i]);
    }*/
}

void Carrot::Engine::recordMainCommandBuffer(size_t i) {
    // main command buffer
    vk::CommandBufferBeginInfo beginInfo{
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
            .pInheritanceInfo = nullptr,
    };

    Console::instance().renderToImGui(*this);
    ImGui::Render();

    mainCommandBuffers[i].begin(beginInfo);
    PrepareVulkanTracy(tracyCtx[i], mainCommandBuffers[i]);

    if(config.runInVR) {
        leftEyeGlobalFrameGraph->execute(newRenderContext(i, Render::Eye::LeftEye), mainCommandBuffers[i]);
        rightEyeGlobalFrameGraph->execute(newRenderContext(i, Render::Eye::RightEye), mainCommandBuffers[i]);
    }
    globalFrameGraph->execute(newRenderContext(i), mainCommandBuffers[i]);

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

void Carrot::Engine::updateUniformBuffer(int imageIndex) {
    if(config.runInVR) {
        static CameraBufferObject leftCBO{};
        static CameraBufferObject rightCBO{};
        leftCBO.update(*cameras[Render::Eye::LeftEye]);
        rightCBO.update(*cameras[Render::Eye::RightEye]);

        vkDriver.getCameraUniformBuffers()[Render::Eye::LeftEye][imageIndex]->directUpload(&leftCBO, sizeof(leftCBO));
        vkDriver.getCameraUniformBuffers()[Render::Eye::RightEye][imageIndex]->directUpload(&rightCBO, sizeof(rightCBO));
    } else {
        static CameraBufferObject cbo{};
        cbo.update(*cameras[Render::Eye::NoVR]);
        vkDriver.getCameraUniformBuffers()[Render::Eye::NoVR][imageIndex]->directUpload(&cbo, sizeof(cbo));
    }
}

void Carrot::Engine::drawFrame(size_t currentFrame) {
    ZoneScoped;

    vk::Result result;
    uint32_t imageIndex;
    {
        ZoneNamedN(__acquire, "Acquire image", true);

        {
            ZoneNamedN(__fences, "Wait fences", true);
            static_cast<void>(getLogicalDevice().waitForFences((*inFlightFences[currentFrame]), true, UINT64_MAX));
            getLogicalDevice().resetFences((*inFlightFences[currentFrame]));
        }

        TracyVulkanCollect(tracyCtx[lastFrameIndex]);

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
    static DebugBufferObject debug{};
    static int32_t gIndex = -1;
    if(hasPreviousFrame() && showGBuffer) {
        Render::Texture* textureToDisplay = nullptr;
        if(ImGui::Begin("GBuffer View")) {
            ImGui::RadioButton("All channels", &gIndex, -1);
            ImGui::RadioButton("Albedo", &gIndex, 0);
            ImGui::RadioButton("Position", &gIndex, 1);
            ImGui::RadioButton("Normals", &gIndex, 2);
            ImGui::RadioButton("Depth", &gIndex, 3);
            ImGui::RadioButton("Raytracing", &gIndex, 4);
            ImGui::RadioButton("UI", &gIndex, 5);
            ImGui::RadioButton("Int Properties", &gIndex, 6);

            vk::Format format = vk::Format::eR32G32B32A32Sfloat;
            if(gIndex == -1) {
                textureToDisplay = imguiTextures[lastFrameIndex].allChannels;
                format = vk::Format::eR8G8B8A8Unorm;
            }
            if(gIndex == 0) {
                textureToDisplay = imguiTextures[lastFrameIndex].albedo;
                format = vk::Format::eR8G8B8A8Unorm;
            }
            if(gIndex == 1) {
                textureToDisplay = imguiTextures[lastFrameIndex].position;
            }
            if(gIndex == 2) {
                textureToDisplay = imguiTextures[lastFrameIndex].normal;
            }
            if(gIndex == 3) {
                textureToDisplay = imguiTextures[lastFrameIndex].depth;
                format = vkDriver.getDepthFormat();
            }
            if(gIndex == 4) {
                textureToDisplay = imguiTextures[lastFrameIndex].raytracing;
                format = vk::Format::eR8G8B8A8Unorm;
            }
            if(gIndex == 5) {
                textureToDisplay = imguiTextures[lastFrameIndex].ui;
                format = vk::Format::eR8G8B8A8Unorm;
            }
            if(gIndex == 6) {
                textureToDisplay = imguiTextures[lastFrameIndex].intProperties;
                format = vk::Format::eR32Sfloat;
            }
            if(textureToDisplay) {
                static vk::ImageLayout layout = vk::ImageLayout::eUndefined;
                auto size = ImGui::GetWindowSize();
/*                renderer.beforeFrameCommand([&](vk::CommandBuffer& cmds) {
                    layout = textureToDisplay->getCurrentImageLayout();
                    textureToDisplay->transitionInline(cmds, vk::ImageLayout::eShaderReadOnlyOptimal);
                });
                renderer.afterFrameCommand([&](vk::CommandBuffer& cmds) {
                    textureToDisplay->transitionInline(cmds, layout);
                });*/
                vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor;
                if(textureToDisplay == imguiTextures[lastFrameIndex].depth) {
                    aspect = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
                }
                ImGui::Image(textureToDisplay->getImguiID(format, aspect), ImVec2(size.x, size.y - ImGui::GetCursorPosY()));
            }
        }
        ImGui::End();
    }

    {
        ZoneScopedN("Prepare frame");

#ifdef ENABLE_VR
        if(config.runInVR) {
            ZoneScopedN("VR start frame");
            vrSession->startFrame();
        }
#endif

        getDebugUniformBuffers()[imageIndex]->directUpload(&debug, sizeof(debug));

        Carrot::Render::Context renderContext = newRenderContext(imageIndex);
        getRayTracer().onFrame(renderContext);
        game->onFrame(renderContext);
    }
    {
        ZoneScopedN("Record main command buffer");
        recordMainCommandBuffer(imageIndex);
    }

    {
        ZoneScopedN("Update uniform buffer");
        updateUniformBuffer(imageIndex);
    }

    {
        ZoneScopedN("Present");

        vector<vk::Semaphore> waitSemaphores = {*imageAvailableSemaphore[currentFrame]};
        vector<vk::PipelineStageFlags> waitStages = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
        vk::Semaphore signalSemaphores[] = {*renderFinishedSemaphore[currentFrame]};

        game->changeGraphicsWaitSemaphores(imageIndex, waitSemaphores, waitStages);

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


            getGraphicsQueue().submit(submitInfo, *inFlightFences[currentFrame]);
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
                DISCARD(vkDriver.getPresentQueue().presentKHR(&presentInfo));
            }
        }

#ifdef ENABLE_VR
        if(config.runInVR) {
            {
                ZoneScopedN("VR render");
                vrSession->present(newRenderContext(imageIndex));
            }
        }
#endif

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
    cout << "========== RESIZE ==========" << std::endl;
    vkDriver.fetchNewFramebufferSize();

    framebufferResized = false;

    getLogicalDevice().waitIdle();

    size_t previousImageCount = getSwapchainImageCount();
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

vk::Queue& Carrot::Engine::getTransferQueue() {
    return vkDriver.getTransferQueue();
}

vk::Queue& Carrot::Engine::getGraphicsQueue() {
    return vkDriver.getGraphicsQueue();
}

vk::Queue& Carrot::Engine::getPresentQueue() {
    return vkDriver.getPresentQueue();
}

set<uint32_t> Carrot::Engine::createGraphicsAndTransferFamiliesSet() {
    return vkDriver.createGraphicsAndTransferFamiliesSet();
}

uint32_t Carrot::Engine::getSwapchainImageCount() {
    return vkDriver.getSwapchainImageCount();
}

vector<shared_ptr<Carrot::Buffer>>& Carrot::Engine::getDebugUniformBuffers() {
    return vkDriver.getDebugUniformBuffers();
}

shared_ptr<Carrot::Material> Carrot::Engine::getOrCreateMaterial(const string& name) {
    auto it = materials.find(name);
    if(it == materials.end()) {
        materials[name] = make_shared<Material>(*this, name);
    }
    return materials[name];
}

void Carrot::Engine::createCameras() {
    auto center = glm::vec3(5*0.5f, 5*0.5f, 0);

    if(config.runInVR) {
        cameras[Render::Eye::LeftEye] = make_unique<Camera>(glm::mat4{1.0f}, glm::mat4{1.0f});
        cameras[Render::Eye::RightEye] = make_unique<Camera>(glm::mat4{1.0f}, glm::mat4{1.0f});
    } else {
        auto camera = make_unique<Camera>(45.0f, vkDriver.getWindowFramebufferExtent().width / (float) vkDriver.getWindowFramebufferExtent().height, 0.1f, 1000.0f);
        camera->getPositionRef() = glm::vec3(center.x, center.y + 1, 5.0f);
        camera->getTargetRef() = center;
        cameras[Render::Eye::NoVR] = std::move(camera);
    }
}

void Carrot::Engine::onMouseMove(double xpos, double ypos, bool updateOnlyDelta) {
    double dx = xpos-mouseX;
    double dy = ypos-mouseY;
    for(auto& callback : mouseDeltaCallbacks) {
        callback(dx, dy);
    }
    if(grabbingCursor) {
        for(auto& callback : mouseDeltaGrabbedCallbacks) {
            callback(dx, dy);
        }
    }
    if(!updateOnlyDelta) {
        for(auto& callback : mousePositionCallbacks) {
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
    return getCamera(Carrot::Render::Eye::NoVR);
}

Carrot::Camera& Carrot::Engine::getCamera(Carrot::Render::Eye eye) {
    assert(cameras.find(eye) != cameras.end());
    return *cameras[eye];
}

void Carrot::Engine::onMouseButton(int button, int action, int mods) {
    for(auto& callback : mouseButtonCallbacks) {
        callback(button, action == GLFW_PRESS || action == GLFW_REPEAT, mods);
    }
}

void Carrot::Engine::onKeyEvent(int key, int scancode, int action, int mods) {
    if(key == GLFW_KEY_GRAVE_ACCENT && action == GLFW_RELEASE) {
        Console::instance().toggleVisibility();
    }

    if(key == GLFW_KEY_ESCAPE) {
        running = false;
    }

    if(key == GLFW_KEY_F2 && action == GLFW_PRESS) {
        takeScreenshot();
    }

    for(auto& callback : keyCallbacks) {
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

void Carrot::Engine::createTracyContexts() {
    for(size_t i = 0; i < getSwapchainImageCount(); i++) {
        tracyCtx.emplace_back(move(make_unique<TracyVulkanContext>(vkDriver.getPhysicalDevice(), getLogicalDevice(), getGraphicsQueue(), getQueueFamilies().graphicsFamily.value())));
    }
}

vk::Queue& Carrot::Engine::getComputeQueue() {
    return vkDriver.getComputeQueue();
}

Carrot::ASBuilder& Carrot::Engine::getASBuilder() {
    return renderer.getASBuilder();
}

void Carrot::Engine::tick(double deltaTime) {
    ZoneScoped;
    game->tick(deltaTime);
}

void Carrot::Engine::takeScreenshot() {
    namespace fs = std::filesystem;

    auto currentTime = chrono::system_clock::now().time_since_epoch().count();
    auto screenshotFolder = fs::current_path() / "screenshots";
    if(!fs::exists(screenshotFolder)) {
        if(!fs::create_directories(screenshotFolder)) {
            throw runtime_error("Could not create screenshot folder");
        }
    }
    auto screenshotPath = screenshotFolder / (to_string(currentTime) + ".png");

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

void Carrot::Engine::setSkybox(Carrot::Skybox::Type type) {
    static vector<SimpleVertex> skyboxVertices = {
            { { 1.0f, -1.0f, -1.0f } },
            { { 1.0f, -1.0f, 1.0f } },
            { { -1.0f, -1.0f, -1.0f } },
            { { -1.0f, -1.0f, 1.0f } },
            { { 1.0f, 1.0f, -1.0f } },
            { { 1.0f, 1.0f, 1.0f } },
            { { -1.0f, 1.0f, -1.0f } },
            { { -1.0f, 1.0f, 1.0f } },
    };
    static vector<uint32_t> skyboxIndices = {
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
            skyboxMesh = make_unique<Mesh>(vkDriver, skyboxVertices, skyboxIndices);
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
    globalFrameGraph->onSwapchainImageCountChange(newCount);

    for(const auto& [name, mat]: materials) {
        mat->onSwapchainImageCountChange(newCount);
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

    globalFrameGraph->onSwapchainSizeChange(newWidth, newHeight);

    for(const auto& [name, mat]: materials) {
        mat->onSwapchainSizeChange(newWidth, newHeight);
    }

    game->onSwapchainSizeChange(newWidth, newHeight);

    updateImGuiTextures(getSwapchainImageCount());
}

void Carrot::Engine::updateImGuiTextures(size_t swapchainLength) {
    imguiTextures.resize(swapchainLength);
    for (int i = 0; i < swapchainLength; ++i) {
        auto& textures = imguiTextures[i];
        textures.allChannels = &globalFrameGraph->getTexture(gResolvePassData.resolved, i);

        textures.albedo = &globalFrameGraph->getTexture(gResolvePassData.albedo, i);

        textures.position = &globalFrameGraph->getTexture(gResolvePassData.positions, i);

        textures.normal = &globalFrameGraph->getTexture(gResolvePassData.normals, i);

        textures.depth = &globalFrameGraph->getTexture(gResolvePassData.depthStencil, i);

        textures.intProperties = &globalFrameGraph->getTexture(gResolvePassData.flags, i);

        textures.raytracing = &globalFrameGraph->getTexture(gResolvePassData.raytracing, i);

    }
}

Carrot::Render::Context Carrot::Engine::newRenderContext(std::size_t swapchainFrameIndex, Render::Eye eye) {
    return Carrot::Render::Context {
            .renderer = renderer,
            .eye = eye,
            .frameCount = frames,
            .swapchainIndex = swapchainFrameIndex,
            .lastSwapchainIndex = lastFrameIndex,
    };
}
