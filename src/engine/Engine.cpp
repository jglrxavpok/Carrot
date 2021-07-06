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

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

static Carrot::RuntimeOption showFPS("Debug/Show FPS", true);

Carrot::Engine::Engine(NakedPtr<GLFWwindow> window): window(window), vkDriver(window), renderer(vkDriver), screenQuad(std::make_unique<Mesh>(vkDriver, vector<ScreenSpaceVertex>{
                                                                                                                                   { { -1, -1} },
                                                                                                                                   { { 1, -1} },
                                                                                                                                   { { 1, 1} },
                                                                                                                                   { { -1, 1} },
                                                                                                                           },
                                                                                                                           vector<uint32_t>{
                                                                                                                                   2,1,0,
                                                                                                                                   3,2,0,
                                                                                                                           })) {
    init();
}

void Carrot::Engine::init() {
    initWindow();

    allocateGraphicsCommandBuffers();
    // quickly render something on screen
    LoadingScreen screen{*this};
    initVulkan();

    struct PresentPassData {
        Render::FrameResource input;
        Render::FrameResource output;
    };

    Render::GraphBuilder mainGraph(vkDriver);
    auto& testTexture = renderer.getOrCreateTexture("default.png");

    auto fullscreenBlit = [this](const vk::RenderPass& pass, const Carrot::Render::FrameData& frame, Carrot::Render::Texture& textureToBlit, vk::CommandBuffer& cmds) {
        auto pipeline = renderer.getOrCreatePipeline("blit");
        vk::DescriptorImageInfo imageInfo {
                .sampler = vkDriver.getLinearSampler(),
                .imageView = textureToBlit.getView(),
                .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
        };

        auto& set = pipeline->getDescriptorSets0()[frame.frameIndex];
        vk::WriteDescriptorSet writeLoadingImage {
                .dstSet = set,
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eSampledImage,
                .pImageInfo = &imageInfo,
        };
        vkDriver.getLogicalDevice().updateDescriptorSets(writeLoadingImage, {});

        pipeline->bind(pass, frame.frameIndex, cmds);
        screenQuad->bind(cmds);
        screenQuad->draw(cmds);
    };

    auto& rtPass = mainGraph.addPass<Carrot::Render::PassData::Raytracing>("tmp",
           [this](Render::GraphBuilder& builder, Render::Pass<Carrot::Render::PassData::Raytracing>& pass, Carrot::Render::PassData::Raytracing& data) {
                pass.rasterized = false;
                vk::Extent3D size {
                    .width = vkDriver.getSwapchainExtent().width,
                    .height = vkDriver.getSwapchainExtent().height,
                    .depth = 1,
                };
                data.output = builder.createRenderTarget(vk::Format::eR8G8B8A8Unorm, size, vk::AttachmentLoadOp::eClear, vk::ClearColorValue(std::array{0,0,0,0}), vk::ImageLayout::eGeneral);
           },
           [this](const Render::CompiledPass& pass, const Render::FrameData& frame, const Carrot::Render::PassData::Raytracing& data, vk::CommandBuffer& buffer) {
                auto& set = renderer.getRayTracer().getRTDescriptorSets()[frame.frameIndex];
                auto& texture = pass.getGraph().getTexture(data.output, frame.frameIndex);
                texture.assumeLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
                texture.transitionInline(buffer, vk::ImageLayout::eGeneral);
                vk::DescriptorImageInfo writeImage {
                    .imageView = texture.getView(),
                    .imageLayout = vk::ImageLayout::eGeneral,
                };
                vk::WriteDescriptorSet updateSet {
                    .dstSet = set,
                    .dstBinding = 1,
                    .descriptorCount = 1,
                    .descriptorType = vk::DescriptorType::eStorageImage,
                    .pImageInfo = &writeImage,
                };
                vkDriver.getLogicalDevice().updateDescriptorSets(updateSet, {});

                getRayTracer().recordCommands(frame.frameIndex, buffer);

                // TODO: make raytracing compatible with RenderGraph
                // TODO: make compute shaders compatible with RenderGraph
           }
    );

    auto& skyboxPass = mainGraph.addPass<Carrot::Render::PassData::Skybox>("skybox",
        [this](Render::GraphBuilder& builder, Render::Pass<Carrot::Render::PassData::Skybox>& pass, Carrot::Render::PassData::Skybox& data) {
            auto& swapchainExtent = vkDriver.getSwapchainExtent();
            data.output = builder.createRenderTarget(vk::Format::eR8G8B8A8Unorm,
                                                     vk::Extent3D{ swapchainExtent.width, swapchainExtent.height, 1 },
                                                     vk::AttachmentLoadOp::eClear,
                                                     vk::ClearColorValue(std::array{0,0,0,1})
                                                     );
        },
        [this](const Render::CompiledPass& pass, const Render::FrameData& frame, const Carrot::Render::PassData::Skybox& data, vk::CommandBuffer& buffer) {
            renderer.bindTexture(*skyboxPipeline, frame, *loadedSkyboxTexture, 0, 0, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::eCube);
            skyboxPipeline->bind(pass.getRenderPass(), frame.frameIndex, buffer);
            skyboxMesh->bind(buffer);
            skyboxMesh->draw(buffer);
        }
    );
    skyboxPass.setCondition([this](const Render::CompiledPass& pass, const Render::FrameData& frame, const Carrot::Render::PassData::Skybox& data) {
        return currentSkybox != Skybox::Type::None;
    });

    auto& imguiPass = renderer.addImGuiPass(mainGraph);

    auto& gbufferPass = getGBuffer().addGBufferPass(mainGraph, [&](const Render::CompiledPass& pass, const Render::FrameData& frame, vk::CommandBuffer& cmds) {
        game->recordGBufferPass(pass.getRenderPass(), frame.frameIndex, cmds);
    });
    auto& gresolvePass = getGBuffer().addGResolvePass(gbufferPass.getData(), rtPass.getData(), imguiPass.getData(), skyboxPass.getData().output, mainGraph);

    gResolvePassData = gresolvePass.getData();
    mainGraph.addPass<PresentPassData>("present",
           [prevPassData = gresolvePass.getData()](Render::GraphBuilder& builder, Render::Pass<PresentPassData>& pass, PresentPassData& data) {
                data.input = builder.read(prevPassData.resolved, vk::ImageLayout::eShaderReadOnlyOptimal);
                data.output = builder.write(builder.getSwapchainImage(), vk::AttachmentLoadOp::eClear, vk::ImageLayout::eColorAttachmentOptimal, vk::ClearColorValue(std::array{0,0,0,0}));
                builder.present(data.output);
           },
           [fullscreenBlit](const Render::CompiledPass& pass, const Render::FrameData& frame, const PresentPassData& data, vk::CommandBuffer& buffer) {
                auto& inputTexture = pass.getGraph().getTexture(data.input, frame.frameIndex);
                fullscreenBlit(pass.getRenderPass(), frame, inputTexture, buffer);
           }
    );

    testGraph = std::move(mainGraph.compile());
    updateImGuiTextures(getSwapchainImageCount());

    initConsole();
}

void Carrot::Engine::initConsole() {
    Console::instance().registerCommands();
}

void Carrot::Engine::run() {
    size_t currentFrame = 0;


    auto previous = chrono::steady_clock::now();
    auto lag = chrono::duration<float>(0.0f);
    const auto timeBetweenUpdates = chrono::duration<float>(1.0f/60.0f); // 60 Hz
    while(running) {
        auto frameStartTime = chrono::steady_clock::now();
        chrono::duration<float> timeElapsed = frameStartTime-previous;
        currentFPS = 1.0f / timeElapsed.count();
        previous = frameStartTime;

        lag += timeElapsed;

        glfwPollEvents();
        if(glfwWindowShouldClose(window.get())) {
            running = false;
        }

        renderer.newFrame();
        ImGui::NewFrame();

        while(lag >= timeBetweenUpdates) {
            tick(timeBetweenUpdates.count());
            lag -= timeBetweenUpdates;
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
    app->onMouseMove(xpos, ypos);
}

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    auto app = reinterpret_cast<Carrot::Engine*>(glfwGetWindowUserPointer(window));
    app->onKeyEvent(key, scancode, action, mods);

    ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
}

void Carrot::Engine::initWindow() {
    glfwSetWindowUserPointer(window.get(), this);
    glfwSetFramebufferSizeCallback(window.get(), windowResize);

    glfwSetCursorPosCallback(window.get(), mouseMove);
    glfwSetKeyCallback(window.get(), keyCallback);

    glfwSetInputMode(window.get(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    grabCursor = true;
}

void Carrot::Engine::initVulkan() {
    resourceAllocator = make_unique<ResourceAllocator>(vkDriver);

    createTracyContexts();

    createCamera();

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

    testGraph->execute(Carrot::Render::FrameData {
        .frameIndex = i,
    }, mainCommandBuffers[i]);

    /*
    {
        PrepareVulkanTracy(tracyCtx[i], mainCommandBuffers[i]);

        TracyVulkanZone(*tracyCtx[i], mainCommandBuffers[i], "Render frame");
        vkDriver.updateViewportAndScissor(mainCommandBuffers[i]);

        vk::ClearValue clearColor = vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,1.0f});
        vk::ClearValue lightingClear = vk::ClearColorValue(std::array{1.0f,1.0f,1.0f,1.0f});
        vk::ClearValue positionClear = vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,0.0f});
        vk::ClearValue uiClear = vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,0.0f});
        vk::ClearValue clearDepth = vk::ClearDepthStencilValue{
                .depth = 1.0f,
                .stencil = 0
        };
        vk::ClearValue clearIntProperties = vk::ClearColorValue();

        vk::ClearValue clearValues[] = {
                clearColor, // final presented color
                clearDepth,
                clearColor, // gbuffer color
                positionClear, // gbuffer view position
                positionClear, // gbuffer view normal,
                lightingClear, // raytraced lighting colors
                clearIntProperties,
        };

        vk::ClearValue imguiClearValues[] = {
                uiClear, // UI contents
        };

        vk::RenderPassBeginInfo imguiRenderPassInfo {
                .renderPass = renderer.getImguiRenderPass(),
                .framebuffer = *renderer.getImguiFramebuffers()[i],
                .renderArea = {
                        .offset = vk::Offset2D{0, 0},
                        .extent = vkDriver.getSwapchainExtent()
                },

                .clearValueCount = 1,
                .pClearValues = imguiClearValues,
        };

        getRayTracer().recordCommands(i, mainCommandBuffers[i]);

        {
            TracyVulkanZone(*tracyCtx[i], mainCommandBuffers[i], "UI pass");
            mainCommandBuffers[i].beginRenderPass(imguiRenderPassInfo, vk::SubpassContents::eInline);
            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), mainCommandBuffers[i]);
            mainCommandBuffers[i].endRenderPass();
        }

        auto& uiTexture = renderer.getUITextures()[i];
        uiTexture->assumeLayout(vk::ImageLayout::eColorAttachmentOptimal);
        uiTexture->transitionInline(mainCommandBuffers[i], vk::ImageLayout::eShaderReadOnlyOptimal);

        vk::RenderPassBeginInfo gRenderPassInfo {
                .renderPass = renderer.getGRenderPass(),
                .framebuffer = *renderer.getSwapchainFramebuffers()[i],
                .renderArea = {
                        .offset = vk::Offset2D{0, 0},
                        .extent = vkDriver.getSwapchainExtent()
                },

                .clearValueCount = 6,
                .pClearValues = clearValues,
        };

        {
            TracyVulkanZone(*tracyCtx[i], mainCommandBuffers[i], "Skybox pass");

            // TODO: custom framebuffer, custom render pass
            if(currentSkybox != Skybox::Type::None) {
                vk::ClearValue skyboxClear[] = {
                    clearColor,
                    clearDepth,
                };

                vk::RenderPassBeginInfo skyboxRenderPassInfo {
                        .renderPass = renderer.getSkyboxRenderPass(),
                        .framebuffer = *renderer.getSkyboxFramebuffers()[i],
                        .renderArea = {
                                .offset = vk::Offset2D{0, 0},
                                .extent = vkDriver.getSwapchainExtent()
                        },

                        .clearValueCount = 2,
                        .pClearValues = skyboxClear,
                };

                renderer.getSkyboxTextures()[i]->transitionInline(mainCommandBuffers[i], vk::ImageLayout::eColorAttachmentOptimal);

                mainCommandBuffers[i].beginRenderPass(skyboxRenderPassInfo, vk::SubpassContents::eSecondaryCommandBuffers);
                mainCommandBuffers[i].executeCommands(skyboxCommandBuffers[i]);
                mainCommandBuffers[i].endRenderPass();

                renderer.getSkyboxTextures()[i]->transitionInline(mainCommandBuffers[i], vk::ImageLayout::eShaderReadOnlyOptimal);
            }
        }

        {
            TracyVulkanZone(*tracyCtx[i], mainCommandBuffers[i], "Render pass 0");

            mainCommandBuffers[i].beginRenderPass(gRenderPassInfo, vk::SubpassContents::eSecondaryCommandBuffers);

            mainCommandBuffers[i].executeCommands(gBufferCommandBuffers[i]);

            mainCommandBuffers[i].nextSubpass(vk::SubpassContents::eSecondaryCommandBuffers);

            mainCommandBuffers[i].executeCommands(gResolveCommandBuffers[i]);

            mainCommandBuffers[i].endRenderPass();
        }
    }
*/
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
    static CameraBufferObject cbo{};

    cbo.update(*camera);
    getCameraUniformBuffers()[imageIndex]->directUpload(&cbo, sizeof(cbo));
}

void Carrot::Engine::drawFrame(size_t currentFrame) {
    ZoneScoped;

    vk::Result result;
    uint32_t imageIndex;
    {
        ZoneNamedN(__acquire, "Acquire image", true);
        static_cast<void>(getLogicalDevice().waitForFences((*inFlightFences[currentFrame]), true, UINT64_MAX));
        getLogicalDevice().resetFences((*inFlightFences[currentFrame]));


        auto nextImage = getLogicalDevice().acquireNextImageKHR(vkDriver.getSwapchain(), UINT64_MAX, *imageAvailableSemaphore[currentFrame], nullptr);
        result = nextImage.result;

        if(result == vk::Result::eErrorOutOfDateKHR) {
            recreateSwapchain();
            return;
        } else if(result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
            throw std::runtime_error("Failed to acquire swap chain image");
        }
        imageIndex = nextImage.value;

        static DebugBufferObject debug{};
        static int32_t gIndex = -1;
        if(hasPreviousFrame()) {
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

                // TODO: transition images to shader readonly before draw, transition back to previous state after rendering
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
                    renderer.beforeFrameCommand([&](vk::CommandBuffer& cmds) {
                        layout = textureToDisplay->getCurrentImageLayout();
                        textureToDisplay->transitionInline(cmds, vk::ImageLayout::eShaderReadOnlyOptimal);
                    });
                    renderer.afterFrameCommand([&](vk::CommandBuffer& cmds) {
                        textureToDisplay->transitionInline(cmds, layout);
                    });
                    vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor;
                    if(textureToDisplay == imguiTextures[lastFrameIndex].depth) {
                        aspect = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
                    }
                    ImGui::Image(textureToDisplay->getImguiID(format, aspect), ImVec2(size.x, size.y - ImGui::GetCursorPosY()));
                }
            }
            ImGui::End();
        }

        getDebugUniformBuffers()[imageIndex]->directUpload(&debug, sizeof(debug));

        game->onFrame(imageIndex);

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

        vk::SubmitInfo submitInfo {
                .waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size()),
                .pWaitSemaphores = waitSemaphores.data(),

                .pWaitDstStageMask = waitStages.data(),

                .commandBufferCount = 1,
                .pCommandBuffers = &mainCommandBuffers[imageIndex],

                .signalSemaphoreCount = 1,
                .pSignalSemaphores = signalSemaphores,
        };

        renderer.preFrame();

        getLogicalDevice().resetFences(*inFlightFences[currentFrame]);
        getGraphicsQueue().submit(submitInfo, *inFlightFences[currentFrame]);

        renderer.postFrame();

        vk::SwapchainKHR swapchains[] = { vkDriver.getSwapchain() };

        vk::PresentInfoKHR presentInfo{
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = signalSemaphores,

                .swapchainCount = 1,
                .pSwapchains = swapchains,
                .pImageIndices = &imageIndex,
                .pResults = nullptr,
        };

        try {
            result = getPresentQueue().presentKHR(presentInfo);
        } catch(vk::OutOfDateKHRError const &e) {
            result = vk::Result::eErrorOutOfDateKHR;
        }
    }


    for(size_t i = 0; i < getSwapchainImageCount(); i++) {
        TracyVulkanCollect(tracyCtx[i]);
    }

    lastFrameIndex = imageIndex;

    if(result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || framebufferResized) {
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
    onSwapchainSizeChange(vkDriver.getSwapchainExtent().width, vkDriver.getSwapchainExtent().height);
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

vector<shared_ptr<Carrot::Buffer>>& Carrot::Engine::getCameraUniformBuffers() {
    return vkDriver.getCameraUniformBuffers();
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

void Carrot::Engine::createCamera() {
    auto center = glm::vec3(5*0.5f, 5*0.5f, 0);

    camera = make_unique<Camera>(45.0f, vkDriver.getSwapchainExtent().width / (float) vkDriver.getSwapchainExtent().height, 0.1f, 1000.0f);
    camera->position = glm::vec3(center.x, center.y + 1, 5.0f);
    camera->target = center;
}

void Carrot::Engine::onMouseMove(double xpos, double ypos) {
    double dx = xpos-mouseX;
    double dy = ypos-mouseY;
    if(game) {
        game->onMouseMove(dx, dy);
    }
    mouseX = xpos;
    mouseY = ypos;
}

Carrot::Camera& Carrot::Engine::getCamera() {
    return *camera;
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

    if(key == GLFW_KEY_G && action == GLFW_PRESS) {
        grabCursor = !grabCursor;
        glfwSetInputMode(window.get(), GLFW_CURSOR, grabCursor ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    }


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

    auto& swapchainExtent = vkDriver.getSwapchainExtent();
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
        loadedSkyboxTexture = std::make_unique<Render::Texture>(Image::cubemapFromFiles(vkDriver, [type](Skybox::Direction dir) {
            return Skybox::getTexturePath(type, dir);
        }));
        loadedSkyboxTexture->name("Current loaded skybox");
        loadedSkyboxTextureView = vkDriver.createImageView(loadedSkyboxTexture->getVulkanImage(),
                                                           vk::Format::eR8G8B8A8Unorm, vk::ImageAspectFlagBits::eColor,
                                                           vk::ImageViewType::eCube, 6
                                                           );
        skyboxPipeline = make_shared<Pipeline>(vkDriver, "resources/pipelines/skybox.json");
        skyboxMesh = make_unique<Mesh>(vkDriver, skyboxVertices, skyboxIndices);
    }
}

void Carrot::Engine::onSwapchainImageCountChange(size_t newCount) {
    vkDriver.onSwapchainImageCountChange(newCount);

    // TODO: rebuild graphs
    // TODO: multi-threading (command pools are threadlocal)
    vkDriver.getLogicalDevice().resetCommandPool(getGraphicsCommandPool());
    allocateGraphicsCommandBuffers();

    renderer.onSwapchainImageCountChange(newCount);

    if(skyboxPipeline) {
        skyboxPipeline->onSwapchainImageCountChange(newCount);
    }
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

    if(skyboxPipeline) {
        skyboxPipeline->onSwapchainSizeChange(newWidth, newHeight);
    }
    for(const auto& [name, mat]: materials) {
        mat->onSwapchainSizeChange(newWidth, newHeight);
    }

    game->onSwapchainSizeChange(newWidth, newHeight);

    // TODO: update render graph

    updateImGuiTextures(getSwapchainImageCount());
}

void Carrot::Engine::updateImGuiTextures(size_t swapchainLength) {
    imguiTextures.resize(swapchainLength);
    for (int i = 0; i < swapchainLength; ++i) {
        auto& textures = imguiTextures[i];
        textures.allChannels = &testGraph->getTexture(gResolvePassData.resolved, i);

        textures.albedo = &testGraph->getTexture(gResolvePassData.albedo, i);

        textures.position = &testGraph->getTexture(gResolvePassData.positions, i);

        textures.normal = &testGraph->getTexture(gResolvePassData.normals, i);

        textures.depth = &testGraph->getTexture(gResolvePassData.depthStencil, i);

        textures.intProperties = &testGraph->getTexture(gResolvePassData.flags, i);

        textures.raytracing = &testGraph->getTexture(gResolvePassData.raytracing, i);

        textures.ui = &testGraph->getTexture(gResolvePassData.ui, i);

    }
}
