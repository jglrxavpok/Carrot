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
#include "game/Game.h"
#include "stb_image_write.h"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

Carrot::Engine::Engine(NakedPtr<GLFWwindow> window): window(window), vkDriver(window) {
    init();
}

void Carrot::Engine::init() {
    initWindow();
    initVulkan();
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

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        while(lag >= timeBetweenUpdates) {
            tick(timeBetweenUpdates.count());
            lag -= timeBetweenUpdates;
        }

        if(ImGui::Begin("FPS Counter", nullptr, ImGuiWindowFlags_::ImGuiWindowFlags_NoCollapse)) {
            ImGui::Text("%f FPS", currentFPS);
        }
        ImGui::End();

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
}

void Carrot::Engine::initWindow() {
    glfwSetWindowUserPointer(window.get(), this);
    glfwSetFramebufferSizeCallback(window.get(), windowResize);

    glfwSetCursorPosCallback(window.get(), mouseMove);
    glfwSetKeyCallback(window.get(), keyCallback);

    glfwSetInputMode(window.get(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    grabCursor = true;

    glfwGetFramebufferSize(window.get(), &framebufferWidth, &framebufferHeight);
}

void Carrot::Engine::initVulkan() {
    resourceAllocator = make_unique<ResourceAllocator>(vkDriver);

    createSwapChain();

    allocateGraphicsCommandBuffers();
    createTracyContexts();

    createRayTracer();
    createUIResources();
    createGBuffer();
    createRenderPasses();
    createFramebuffers();
    initImgui();
    createUniformBuffers();
    createSamplers();
    gBuffer->loadResolvePipeline();

    createDefaultTexture();
    createCamera();

    raytracer->createDescriptorSets();
    raytracer->createPipeline();
    raytracer->createShaderBindingTable();

    initGame();

    raytracer->finishInit();

    for(size_t i = 0; i < getSwapchainImageCount(); i++) {
        recordSecondaryCommandBuffers(i); // g-buffer pass and rt pass
    }
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
    swapchain.reset();
    getVkInstance().destroySurfaceKHR(vkDriver.getSurface(), vkDriver.getAllocationCallbacks());
}

void Carrot::Engine::createUIResources() {
    vk::DescriptorPoolSize pool_sizes[] =
            {
                    { vk::DescriptorType::eSampler, 1000 },
                    { vk::DescriptorType::eCombinedImageSampler, 1000 },
                    { vk::DescriptorType::eSampledImage, 1000 },
                    { vk::DescriptorType::eStorageImage, 1000 },
                    { vk::DescriptorType::eUniformTexelBuffer, 1000 },
                    { vk::DescriptorType::eStorageTexelBuffer, 1000 },
                    { vk::DescriptorType::eUniformBuffer, 1000 },
                    { vk::DescriptorType::eStorageBuffer, 1000 },
                    { vk::DescriptorType::eUniformBufferDynamic, 1000 },
                    { vk::DescriptorType::eStorageBufferDynamic, 1000 },
                    { vk::DescriptorType::eInputAttachment, 1000 }
            };
    vk::DescriptorPoolCreateInfo pool_info = {};
    pool_info.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
    pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;
    imguiDescriptorPool = getLogicalDevice().createDescriptorPoolUnique(pool_info, vkDriver.getAllocationCallbacks());



    uiImages.resize(getSwapchainImageCount());
    uiImageViews.resize(getSwapchainImageCount());
    for (int i = 0; i < getSwapchainImageCount(); ++i) {
        uiImages[i] = move(make_unique<Image>(vkDriver,
                                              vk::Extent3D{swapchainExtent.width, swapchainExtent.height, 1},
                                                  vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
                                              vk::Format::eR8G8B8A8Unorm));

        auto view = uiImages[i]->createImageView();
        uiImageViews[i] = std::move(view);
    }
}

vk::SurfaceFormatKHR Carrot::Engine::chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& formats) {
    for(const auto& available : formats) {
        if(available.format == vk::Format::eA8B8G8R8SrgbPack32 && available.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            return available;
        }
    }

    // TODO: rank based on format and color space

    return formats[0];
}

vk::PresentModeKHR Carrot::Engine::chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& presentModes) {
    for(const auto& mode : presentModes) {
        if(mode == vk::PresentModeKHR::eMailbox) {
            return mode;
        }
    }

    // only one guaranteed
    return vk::PresentModeKHR::eFifo;
}

vk::Extent2D Carrot::Engine::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities) {
    if(capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent; // no choice
    } else {
        int width, height;
        glfwGetFramebufferSize(window.get(), &width, &height);

        vk::Extent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height),
        };

        actualExtent.width = max(capabilities.minImageExtent.width, min(capabilities.maxImageExtent.width, actualExtent.width));
        actualExtent.height = max(capabilities.minImageExtent.height, min(capabilities.maxImageExtent.height, actualExtent.height));

        return actualExtent;
    }
}

void Carrot::Engine::createSwapChain() {
    SwapChainSupportDetails swapChainSupport = vkDriver.querySwapChainSupport(getPhysicalDevice());

    vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    vk::PresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    vk::Extent2D swapchainExtent = chooseSwapExtent(swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount +1;
    // maxImageCount == 0 means we can request any number of image
    if(swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        // ensure we don't ask for more images than the device will be able to provide
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR createInfo{
        .surface = vkDriver.getSurface(),
        .minImageCount = imageCount,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = swapchainExtent,
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment, // used for rendering

        .preTransform = swapChainSupport.capabilities.currentTransform,

        // don't try to blend with background of other windows
        .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,

        .presentMode = presentMode,
        .clipped = VK_TRUE,

        .oldSwapchain = nullptr,
    };

    // image info

    uint32_t indices[] = { getQueueFamilies().graphicsFamily.value(), getQueueFamilies().presentFamily.value() };

    if(getQueueFamilies().presentFamily != getQueueFamilies().graphicsFamily) {
        // image will be shared between the 2 queues, without explicit transfers
        createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = indices;
    } else {
        // always on same queue, no need to share

        createInfo.imageSharingMode = vk::SharingMode::eExclusive;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }

    swapchain = getLogicalDevice().createSwapchainKHRUnique(createInfo, vkDriver.getAllocationCallbacks());

    const auto& swapchainDeviceImages = getLogicalDevice().getSwapchainImagesKHR(*swapchain);
    swapchainImages.clear();
    for(const auto& image : swapchainDeviceImages) {
        swapchainImages.push_back(image);
    }

    this->swapchainImageFormat = surfaceFormat.format;
    this->swapchainExtent = swapchainExtent;

    depthFormat = findDepthFormat();

    createSwapChainImageViews();
}

void Carrot::Engine::createSwapChainImageViews() {
    swapchainImageViews.resize(swapchainImages.size());

    for(size_t index = 0; index < swapchainImages.size(); index++) {
        auto view = Engine::createImageView(swapchainImages[index], swapchainImageFormat);
        swapchainImageViews[index] = std::move(view);
    }
}

vk::UniqueImageView Carrot::Engine::createImageView(const vk::Image& image, vk::Format imageFormat, vk::ImageAspectFlags aspectMask) {
    return vkDriver.createImageView(image, imageFormat, aspectMask);
}

void Carrot::Engine::createRenderPasses() {
    gRenderPass = gBuffer->createRenderPass();

    vk::AttachmentDescription attachments[] = {
            {
                .format = vk::Format::eR8G8B8A8Unorm,
                .samples = vk::SampleCountFlagBits::e1,
                .loadOp = vk::AttachmentLoadOp::eClear,
                .storeOp = vk::AttachmentStoreOp::eStore,
                .initialLayout = vk::ImageLayout::eUndefined,
                .finalLayout = vk::ImageLayout::eColorAttachmentOptimal,
            }
    };

    vk::SubpassDependency dependencies[] = {
            {
                    .srcSubpass = VK_SUBPASS_EXTERNAL,
                    .dstSubpass = 0,

                    .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                                    vk::PipelineStageFlagBits::eEarlyFragmentTests,
                    // TODO: .srcAccessMask = 0,

                    .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                                    vk::PipelineStageFlagBits::eEarlyFragmentTests,
                    .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
            }
    };

    vk::AttachmentReference colorAttachment = {
            .attachment = 0,
            .layout = vk::ImageLayout::eColorAttachmentOptimal,
    };

    vk::SubpassDescription subpasses[] = {
            {
                    .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
                    .inputAttachmentCount = 0,
                    .pInputAttachments = nullptr,

                    .colorAttachmentCount = 1,
                    // index in this array is used by `layout(location = 0)` inside shaders
                    .pColorAttachments = &colorAttachment,
                    .pDepthStencilAttachment = nullptr,

                    .preserveAttachmentCount = 0,
            }
    };

    vk::RenderPassCreateInfo imguiRenderPassInfo {
        .attachmentCount = 1,
        .pAttachments = attachments,
        .subpassCount = 1,
        .pSubpasses = subpasses,
        .dependencyCount = 1,
        .pDependencies = dependencies,
    };
    imguiRenderPass = getLogicalDevice().createRenderPassUnique(imguiRenderPassInfo);
}

void Carrot::Engine::createFramebuffers() {
    swapchainFramebuffers.resize(swapchainImages.size());
    imguiFramebuffers.resize(swapchainImages.size());

    for (size_t i = 0; i < swapchainImageViews.size(); ++i) {
        vector<vk::ImageView> attachments = {
                *swapchainImageViews[i],
        };

        gBuffer->addFramebufferAttachments(i, attachments);

        vk::FramebufferCreateInfo swapchainFramebufferInfo {
                .renderPass = *gRenderPass,
                .attachmentCount = static_cast<uint32_t>(attachments.size()),
                .pAttachments = attachments.data(),
                .width = swapchainExtent.width,
                .height = swapchainExtent.height,
                .layers = 1,
        };

        swapchainFramebuffers[i] = std::move(getLogicalDevice().createFramebufferUnique(swapchainFramebufferInfo, vkDriver.getAllocationCallbacks()));

        vk::ImageView imguiAttachment[] = {
            *uiImageViews[i]
        };
        vk::FramebufferCreateInfo imguiFramebufferInfo {
                .renderPass = *imguiRenderPass,
                .attachmentCount = 1,
                .pAttachments = imguiAttachment,
                .width = swapchainExtent.width,
                .height = swapchainExtent.height,
                .layers = 1,
        };
        imguiFramebuffers[i] = std::move(getLogicalDevice().createFramebufferUnique(imguiFramebufferInfo, vkDriver.getAllocationCallbacks()));
    }
}

static void check_vk_result(VkResult err)
{
    if (err == 0)
        return;
    fprintf(stderr, "[vulkan-imgui] Error: VkResult = %d\n", err);
    if (err < 0)
        abort();
}

void Carrot::Engine::initImgui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForVulkan(window.get(), true);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = getVkInstance();
    init_info.PhysicalDevice = getPhysicalDevice();
    init_info.Device = getLogicalDevice();
    init_info.QueueFamily = getQueueFamilies().graphicsFamily.value();
    init_info.Queue = getGraphicsQueue();
    init_info.PipelineCache = nullptr;
    init_info.DescriptorPool = *imguiDescriptorPool;
    init_info.Allocator = nullptr;

    SwapChainSupportDetails swapChainSupport = vkDriver.querySwapChainSupport(getPhysicalDevice());

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount +1;
    // maxImageCount == 0 means we can request any number of image
    if(swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        // ensure we don't ask for more images than the device will be able to provide
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    init_info.MinImageCount = swapChainSupport.capabilities.minImageCount;
    init_info.ImageCount = imageCount;
    init_info.CheckVkResultFn = check_vk_result;
    ImGui_ImplVulkan_Init(&init_info, *imguiRenderPass);

    // Upload fonts
    performSingleTimeGraphicsCommands([&](vk::CommandBuffer& buffer) {
        ImGui_ImplVulkan_CreateFontsTexture(buffer);
    });
}

void Carrot::Engine::createRayTracer() {
    raytracer = make_unique<RayTracer>(*this);
    asBuilder = make_unique<ASBuilder>(*this);
}

void Carrot::Engine::createGBuffer() {
    gBuffer = make_unique<GBuffer>(*this, *raytracer);
}

void Carrot::Engine::recordSecondaryCommandBuffers(size_t frameIndex) {
    recordGBufferPass(frameIndex, gBufferCommandBuffers[frameIndex]);

    vk::CommandBufferInheritanceInfo gResolveInheritance {
            .renderPass = *this->gRenderPass,
            .subpass = RenderPasses::GResolveSubPassIndex,
            .framebuffer = *this->swapchainFramebuffers[frameIndex],
    };
    gBuffer->recordResolvePass(frameIndex, gResolveCommandBuffers[frameIndex], &gResolveInheritance);
}

void Carrot::Engine::recordGBufferPass(size_t frameIndex, vk::CommandBuffer& gBufferCommandBuffer) {
    vk::CommandBufferInheritanceInfo inheritance {
            .renderPass = *this->gRenderPass,
            .subpass = RenderPasses::GBufferSubPassIndex,
            .framebuffer = *this->swapchainFramebuffers[frameIndex],
    };
    vk::CommandBufferBeginInfo beginInfo{
            .flags = vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse,
            .pInheritanceInfo = &inheritance,
    };

    gBufferCommandBuffer.begin(beginInfo);
    {
        this->updateViewportAndScissor(gBufferCommandBuffer);

        this->game->recordGBufferPass(frameIndex, gBufferCommandBuffer);
    }
    gBufferCommandBuffer.end();
}

void Carrot::Engine::recordMainCommandBuffer(size_t i) {
    // main command buffer
    vk::CommandBufferBeginInfo beginInfo{
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
            .pInheritanceInfo = nullptr,
    };

    mainCommandBuffers[i].begin(beginInfo);
    {
        PrepareVulkanTracy(tracyCtx[i], mainCommandBuffers[i]);

        TracyVulkanZone(*tracyCtx[i], mainCommandBuffers[i], "Render frame");
        updateViewportAndScissor(mainCommandBuffers[i]);

        vk::ClearValue clearColor = vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,1.0f});
        vk::ClearValue lightingClear = vk::ClearColorValue(std::array{1.0f,1.0f,1.0f,1.0f});
        vk::ClearValue positionClear = vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,0.0f});
        vk::ClearValue uiClear = vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,0.0f});
        vk::ClearValue clearDepth = vk::ClearDepthStencilValue{
                .depth = 1.0f,
                .stencil = 0
        };

        vk::ClearValue clearValues[] = {
                clearColor, // final presented color
                clearDepth,
                clearColor, // gbuffer color
                positionClear, // gbuffer view position
                positionClear, // gbuffer view normal,
                lightingClear, // raytraced lighting colors
        };

        vk::ClearValue imguiClearValues[] = {
                uiClear, // UI contents
        };

        vk::RenderPassBeginInfo imguiRenderPassInfo {
                .renderPass = *imguiRenderPass,
                .framebuffer = *imguiFramebuffers[i],
                .renderArea = {
                        .offset = vk::Offset2D{0, 0},
                        .extent = swapchainExtent
                },

                .clearValueCount = 1,
                .pClearValues = imguiClearValues,
        };

        raytracer->recordCommands(i, mainCommandBuffers[i]);

        {
            TracyVulkanZone(*tracyCtx[i], mainCommandBuffers[i], "UI pass");
            mainCommandBuffers[i].beginRenderPass(imguiRenderPassInfo, vk::SubpassContents::eInline);
            ImGui::Render();
            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), mainCommandBuffers[i]);
            mainCommandBuffers[i].endRenderPass();
        }

        uiImages[i]->transitionLayoutInline(mainCommandBuffers[i], vk::Format::eR8G8B8A8Unorm, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

        vk::RenderPassBeginInfo gRenderPassInfo {
                .renderPass = *gRenderPass,
                .framebuffer = *swapchainFramebuffers[i],
                .renderArea = {
                        .offset = vk::Offset2D{0, 0},
                        .extent = swapchainExtent
                },

                .clearValueCount = 6,
                .pClearValues = clearValues,
        };

        static bool onlyRaytracing = true;


        {
            TracyVulkanZone(*tracyCtx[i], mainCommandBuffers[i], "Render pass 0");

            mainCommandBuffers[i].beginRenderPass(gRenderPassInfo, vk::SubpassContents::eSecondaryCommandBuffers);

            mainCommandBuffers[i].executeCommands(gBufferCommandBuffers[i]);

            mainCommandBuffers[i].nextSubpass(vk::SubpassContents::eSecondaryCommandBuffers);

            mainCommandBuffers[i].executeCommands(gResolveCommandBuffers[i]);

            mainCommandBuffers[i].endRenderPass();

        }
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

    vk::CommandBufferAllocateInfo uiAllocInfo {
            .commandPool = getGraphicsCommandPool(),
            .level = vk::CommandBufferLevel::eSecondary,
            .commandBufferCount = static_cast<uint32_t>(getSwapchainImageCount()),
    };
    this->uiCommandBuffers = getLogicalDevice().allocateCommandBuffers(uiAllocInfo);
}

void Carrot::Engine::updateUniformBuffer(int imageIndex) {
    static CameraBufferObject cbo{};

    cbo.update(*camera);
    cameraUniformBuffers[imageIndex]->directUpload(&cbo, sizeof(cbo));
}

void Carrot::Engine::drawFrame(size_t currentFrame) {
    ZoneScoped;

    vk::Result result;
    uint32_t imageIndex;
    {
        ZoneNamedN(__acquire, "Acquire image", true);
        static_cast<void>(getLogicalDevice().waitForFences((*inFlightFences[currentFrame]), true, UINT64_MAX));
        getLogicalDevice().resetFences((*inFlightFences[currentFrame]));


        auto nextImage = getLogicalDevice().acquireNextImageKHR(*swapchain, UINT64_MAX, *imageAvailableSemaphore[currentFrame], nullptr);
        result = nextImage.result;

        if(result == vk::Result::eErrorOutOfDateKHR) {
            recreateSwapchain();
            return;
        } else if(result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
            throw std::runtime_error("Failed to acquire swap chain image");
        }
        imageIndex = nextImage.value;

        static DebugBufferObject debug{};
        if(ImGui::Begin("GBuffer View"))
        {
            auto* gIndex = reinterpret_cast<int32_t*>(&debug.gChannel);

            ImGui::RadioButton("All channels", gIndex, -1);
            ImGui::RadioButton("Albedo", gIndex, 0);
            ImGui::RadioButton("Position", gIndex, 1);
            ImGui::RadioButton("Normals", gIndex, 2);
            ImGui::RadioButton("Depth", gIndex, 3);
            ImGui::RadioButton("Raytracing", gIndex, 4);
            ImGui::RadioButton("UI", gIndex, 5);
        }
        ImGui::End();

        debugUniformBuffers[imageIndex]->directUpload(&debug, sizeof(debug));

        game->onFrame(imageIndex);

        recordMainCommandBuffer(imageIndex);
    }

    {
        ZoneScopedN("Update uniform buffer");
        updateUniformBuffer(imageIndex);
    }

/*    if(imagesInFlight[imageIndex] != nullptr) {
        getLogicalDevice().waitForFences(*imagesInFlight[imageIndex], true, UINT64_MAX);
    }
    imagesInFlight[imageIndex] = inFlightFences[currentFrame];*/

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


        getLogicalDevice().resetFences(*inFlightFences[currentFrame]);
        getGraphicsQueue().submit(submitInfo, *inFlightFences[currentFrame]);

        vk::SwapchainKHR swapchains[] = { *swapchain };

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
}

void Carrot::Engine::createSynchronizationObjects() {
    imageAvailableSemaphore.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphore.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    imagesInFlight.resize(swapchainImages.size());

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
    glfwGetFramebufferSize(window.get(), &framebufferWidth, &framebufferHeight);
    while(framebufferWidth == 0 || framebufferHeight == 0) {
        glfwGetFramebufferSize(window.get(), &framebufferWidth, &framebufferHeight);
        glfwWaitEvents();
    }

    framebufferResized = false;

    getLogicalDevice().waitIdle();

    cleanupSwapchain();

    createSwapChain();
    createUniformBuffers();
    raytracer->onSwapchainRecreation();
    gBuffer->onSwapchainRecreation();
    createRenderPasses();
    createFramebuffers();
    gBuffer->loadResolvePipeline();
    for(const auto& [name, pipeline] : pipelines) {
        pipeline->recreateDescriptorPool(swapchainFramebuffers.size());
    }
    // TODO: update material descriptor sets
    // TODO: update game swapchain-dependent content
    allocateGraphicsCommandBuffers();
    for(size_t i = 0; i < getSwapchainImageCount(); i++) {
        recordSecondaryCommandBuffers(i); // g-buffer pass and rt pass
    }
}

void Carrot::Engine::cleanupSwapchain() {
    swapchainFramebuffers.clear();
    getLogicalDevice().freeCommandBuffers(getGraphicsCommandPool(), mainCommandBuffers);
    mainCommandBuffers.clear();

    gRenderPass.reset();
    swapchainImageViews.clear();
    swapchain.reset();
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
    return vkDriver.getTransferCommandPool();
}

vk::CommandPool& Carrot::Engine::getGraphicsCommandPool() {
    return vkDriver.getGraphicsCommandPool();
}

vk::CommandPool& Carrot::Engine::getComputeCommandPool() {
    return vkDriver.getComputeCommandPool();
}

vk::Queue Carrot::Engine::getTransferQueue() {
    return vkDriver.getTransferQueue();
}

vk::Queue Carrot::Engine::getGraphicsQueue() {
    return vkDriver.getGraphicsQueue();
}

vk::Queue Carrot::Engine::getPresentQueue() {
    return vkDriver.getPresentQueue();
}

void Carrot::Engine::createUniformBuffers() {
    vk::DeviceSize bufferSize = sizeof(Carrot::CameraBufferObject);
    cameraUniformBuffers.resize(swapchainFramebuffers.size(), nullptr);

    for(size_t i = 0; i < swapchainFramebuffers.size(); i++) {
        cameraUniformBuffers[i] = getResourceAllocator().allocateDedicatedBuffer(
                                                              bufferSize,
                                                              vk::BufferUsageFlagBits::eUniformBuffer,
                                                              vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible,
                                                              createGraphicsAndTransferFamiliesSet());
    }

    vk::DeviceSize debugBufferSize = sizeof(Carrot::DebugBufferObject);
    debugUniformBuffers.resize(swapchainFramebuffers.size(), nullptr);

    for(size_t i = 0; i < swapchainFramebuffers.size(); i++) {
        debugUniformBuffers[i] = make_unique<Carrot::Buffer>(vkDriver,
                                                             debugBufferSize,
                                                             vk::BufferUsageFlagBits::eUniformBuffer,
                                                              vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible,
                                                             createGraphicsAndTransferFamiliesSet());
    }
}

set<uint32_t> Carrot::Engine::createGraphicsAndTransferFamiliesSet() {
    return {
        getQueueFamilies().graphicsFamily.value(),
        getQueueFamilies().transferFamily.value(),
    };
}

vk::Format Carrot::Engine::findSupportedFormat(const vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features) {
    for(auto& format : candidates) {
        vk::FormatProperties properties = getPhysicalDevice().getFormatProperties(format);

        if(tiling == vk::ImageTiling::eLinear && (properties.linearTilingFeatures & features) == features) {
            return format;
        }

        if(tiling == vk::ImageTiling::eOptimal && (properties.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    throw runtime_error("Could not find supported format");
}

vk::Format Carrot::Engine::findDepthFormat() {
    return findSupportedFormat(
            {vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
            vk::ImageTiling::eOptimal,
            vk::FormatFeatureFlagBits::eDepthStencilAttachment);
}

void Carrot::Engine::createSamplers() {
    nearestRepeatSampler = getLogicalDevice().createSamplerUnique({
                                                         .magFilter = vk::Filter::eNearest,
                                                         .minFilter = vk::Filter::eNearest,
                                                         .mipmapMode = vk::SamplerMipmapMode::eNearest,
                                                         .addressModeU = vk::SamplerAddressMode::eRepeat,
                                                         .addressModeV = vk::SamplerAddressMode::eRepeat,
                                                         .addressModeW = vk::SamplerAddressMode::eRepeat,
                                                         .anisotropyEnable = true,
                                                         .maxAnisotropy = 16.0f,
                                                         .unnormalizedCoordinates = false,
                                                 }, vkDriver.getAllocationCallbacks());

    linearRepeatSampler = getLogicalDevice().createSamplerUnique({
                                                        .magFilter = vk::Filter::eLinear,
                                                        .minFilter = vk::Filter::eLinear,
                                                        .mipmapMode = vk::SamplerMipmapMode::eLinear,
                                                        .addressModeU = vk::SamplerAddressMode::eRepeat,
                                                        .addressModeV = vk::SamplerAddressMode::eRepeat,
                                                        .addressModeW = vk::SamplerAddressMode::eRepeat,
                                                        .anisotropyEnable = true,
                                                        .maxAnisotropy = 16.0f,
                                                        .unnormalizedCoordinates = false,
                                                }, vkDriver.getAllocationCallbacks());
}

void Carrot::Engine::updateViewportAndScissor(vk::CommandBuffer& commands) {
    commands.setViewport(0, vk::Viewport{
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>(framebufferWidth),
            .height = static_cast<float>(framebufferHeight),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
    });

    commands.setScissor(0, vk::Rect2D {
            .offset = {0,0},
            .extent = {
                    static_cast<uint32_t>(framebufferWidth),
                    static_cast<uint32_t>(framebufferHeight),
            },
    });
}

shared_ptr<Carrot::Pipeline> Carrot::Engine::getOrCreatePipeline(const string& name) {
    auto it = pipelines.find(name);
    if(it == pipelines.end()) {
        pipelines[name] = make_shared<Pipeline>(*this, gRenderPass, name);
    }
    return pipelines[name];
}

unique_ptr<Carrot::Image>& Carrot::Engine::getOrCreateTexture(const string& textureName) {
    auto it = textureImages.find(textureName);
    if(it == textureImages.end()) {
        auto texture = Image::fromFile(vkDriver, "resources/textures/" + textureName);
        texture->name(textureName);
        textureImages[textureName] = move(texture);
    }
    return textureImages[textureName];
}

vk::UniqueImageView& Carrot::Engine::getOrCreateTextureView(const string& textureName) {
    auto it = textureImageViews.find(textureName);
    if(it == textureImageViews.end()) {
        auto& texture = getOrCreateTexture(textureName);
        auto textureView = texture->createImageView();
        textureImageViews[textureName] = move(textureView);
    }
    return textureImageViews[textureName];
}

uint32_t Carrot::Engine::getSwapchainImageCount() {
    return swapchainImages.size();
}

vector<shared_ptr<Carrot::Buffer>>& Carrot::Engine::getCameraUniformBuffers() {
    return cameraUniformBuffers;
}

vector<shared_ptr<Carrot::Buffer>>& Carrot::Engine::getDebugUniformBuffers() {
    return debugUniformBuffers;
}

const vk::UniqueSampler& Carrot::Engine::getLinearSampler() {
    return linearRepeatSampler;
}

void Carrot::Engine::createDefaultTexture() {
    defaultImage = Image::fromFile(vkDriver, "resources/textures/default.png");
    defaultImageView = move(defaultImage->createImageView());
}

vk::UniqueHandle<vk::ImageView, vk::DispatchLoaderDynamic>& Carrot::Engine::getDefaultImageView() {
    return defaultImageView;
}

shared_ptr<Carrot::Material> Carrot::Engine::getOrCreateMaterial(const string& name) {
    auto it = materials.find(name);
    if(it == materials.end()) {
        materials[name] = make_shared<Material>(*this, name);
    }
    return materials[name];
}

void Carrot::Engine::initGame() {
    game = make_unique<Game::Game>(*this);
}

void Carrot::Engine::createCamera() {
    auto center = glm::vec3(5*0.5f, 5*0.5f, 0);

    camera = make_unique<Camera>(45.0f, swapchainExtent.width / (float) swapchainExtent.height, 0.1f, 1000.0f);
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
        tracyCtx.emplace_back(move(make_unique<TracyVulkanContext>(getPhysicalDevice(), getLogicalDevice(), getGraphicsQueue(), getQueueFamilies().graphicsFamily.value())));
    }
}

vk::Queue Carrot::Engine::getComputeQueue() {
    return vkDriver.getComputeQueue();
}

vk::Extent2D Carrot::Engine::getSwapchainExtent() const {
    return swapchainExtent;
}

vk::Format Carrot::Engine::getDepthFormat() const {
    return depthFormat;
}

vk::Format Carrot::Engine::getSwapchainImageFormat() const {
    return swapchainImageFormat;
}

vk::PhysicalDevice& Carrot::Engine::getPhysicalDevice() {
    return vkDriver.getPhysicalDevice();
}

Carrot::ASBuilder& Carrot::Engine::getASBuilder() {
    return *asBuilder;
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

    auto lastImage = swapchainImages[lastFrameIndex];

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
        Image::transition(lastImage, commands, swapchainImageFormat, vk::ImageLayout::ePresentSrcKHR, vk::ImageLayout::eTransferSrcOptimal);
        screenshotImage.transitionLayoutInline(commands, vk::Format::eR8G8B8A8Unorm, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

        commands.blitImage(lastImage, vk::ImageLayout::eTransferSrcOptimal, screenshotImage.getVulkanImage(), vk::ImageLayout::eTransferDstOptimal, vk::ImageBlit {
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

vector<vk::UniqueImageView>& Carrot::Engine::getUIImageViews() {
    return uiImageViews;
}
