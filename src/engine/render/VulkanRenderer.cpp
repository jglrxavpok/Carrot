//
// Created by jglrxavpok on 13/03/2021.
//

#include "VulkanRenderer.h"
#include "GBuffer.h"
#include "engine/render/raytracing/ASBuilder.h"
#include "engine/render/raytracing/RayTracer.h"
#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#include "backends/imgui_impl_glfw.h"

Carrot::VulkanRenderer::VulkanRenderer(VulkanDriver& driver): driver(driver) {
    createRayTracer();
    createUIResources();
    createSkyboxResources();
    createGBuffer();
    createRenderPasses();
    createFramebuffers();

    initImgui();
    gBuffer->loadResolvePipeline();
}

shared_ptr<Carrot::Pipeline> Carrot::VulkanRenderer::getOrCreatePipeline(const string& name) {
    auto it = pipelines.find(name);
    if(it == pipelines.end()) {
        pipelines[name] = make_shared<Pipeline>(driver, *gRenderPass, "resources/pipelines/"+name+".json");
    }
    return pipelines[name];
}

unique_ptr<Carrot::Image>& Carrot::VulkanRenderer::getOrCreateTexture(const string& textureName) {
    auto it = textureImages.find(textureName);
    if(it == textureImages.end()) {
        auto texture = Image::fromFile(driver, "resources/textures/" + textureName);
        texture->name(textureName);
        textureImages[textureName] = move(texture);
    }
    return textureImages[textureName];
}

void Carrot::VulkanRenderer::recreateDescriptorPools(size_t frameCount) {
    for(const auto& [name, pipeline] : pipelines) {
        pipeline->recreateDescriptorPool(frameCount);
    }
}

vk::UniqueImageView& Carrot::VulkanRenderer::getOrCreateTextureView(const string& textureName) {
    auto it = textureImageViews.find(textureName);
    if(it == textureImageViews.end()) {
        auto& texture = getOrCreateTexture(textureName);
        auto textureView = texture->createImageView();
        textureImageViews[textureName] = move(textureView);
    }
    return textureImageViews[textureName];
}

void Carrot::VulkanRenderer::createUIResources() {
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
    imguiDescriptorPool = driver.getLogicalDevice().createDescriptorPoolUnique(pool_info, driver.getAllocationCallbacks());

    createUIImages();
}

void Carrot::VulkanRenderer::createRenderPasses() {
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

    vk::RenderPassCreateInfo singleColorWriteRenderPassInfo {
            .attachmentCount = 1,
            .pAttachments = attachments,
            .subpassCount = 1,
            .pSubpasses = subpasses,
            .dependencyCount = 1,
            .pDependencies = dependencies,
    };
    imguiRenderPass = driver.getLogicalDevice().createRenderPassUnique(singleColorWriteRenderPassInfo);

    createSkyboxRenderPass();
}

void Carrot::VulkanRenderer::createSkyboxRenderPass() {
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

    vk::RenderPassCreateInfo singleColorWriteRenderPassInfo {
            .attachmentCount = 1,
            .pAttachments = attachments,
            .subpassCount = 1,
            .pSubpasses = subpasses,
            .dependencyCount = 1,
            .pDependencies = dependencies,
    };

    skyboxRenderPass = driver.getLogicalDevice().createRenderPassUnique(singleColorWriteRenderPassInfo);
}

void Carrot::VulkanRenderer::createFramebuffers() {
    swapchainFramebuffers.resize(driver.getSwapchainImageCount());
    imguiFramebuffers.resize(driver.getSwapchainImageCount());
    skyboxFramebuffers.resize(driver.getSwapchainImageCount());

    for (size_t i = 0; i < driver.getSwapchainImageCount(); ++i) {
        vector<vk::ImageView> attachments = {
                *driver.getSwapchainImageViews()[i],
        };

        gBuffer->addFramebufferAttachments(i, attachments);

        vk::FramebufferCreateInfo swapchainFramebufferInfo {
                .renderPass = *gRenderPass,
                .attachmentCount = static_cast<uint32_t>(attachments.size()),
                .pAttachments = attachments.data(),
                .width = driver.getSwapchainExtent().width,
                .height = driver.getSwapchainExtent().height,
                .layers = 1,
        };

        swapchainFramebuffers[i] = std::move(driver.getLogicalDevice().createFramebufferUnique(swapchainFramebufferInfo, driver.getAllocationCallbacks()));

        vk::ImageView imguiAttachment[] = {
                *uiImageViews[i]
        };
        vk::FramebufferCreateInfo imguiFramebufferInfo {
                .renderPass = *imguiRenderPass,
                .attachmentCount = 1,
                .pAttachments = imguiAttachment,
                .width = driver.getSwapchainExtent().width,
                .height = driver.getSwapchainExtent().height,
                .layers = 1,
        };
        imguiFramebuffers[i] = std::move(driver.getLogicalDevice().createFramebufferUnique(imguiFramebufferInfo, driver.getAllocationCallbacks()));

        vk::ImageView skyboxAttachment[] = {
                *skyboxImageViews[i]
        };
        vk::FramebufferCreateInfo skyboxFramebufferInfo {
                .renderPass = *skyboxRenderPass,
                .attachmentCount = 1,
                .pAttachments = skyboxAttachment,
                .width = driver.getSwapchainExtent().width,
                .height = driver.getSwapchainExtent().height,
                .layers = 1,
        };
        skyboxFramebuffers[i] = std::move(driver.getLogicalDevice().createFramebufferUnique(skyboxFramebufferInfo, driver.getAllocationCallbacks()));
    }
}

void Carrot::VulkanRenderer::createGBuffer() {
    gBuffer = make_unique<GBuffer>(*this, *raytracer);
}

void Carrot::VulkanRenderer::createRayTracer() {
    raytracer = make_unique<RayTracer>(*this);
    asBuilder = make_unique<ASBuilder>(*this);
}

static void check_vk_result(VkResult err)
{
    if (err == 0)
        return;
    fprintf(stderr, "[vulkan-imgui] Error: VkResult = %d\n", err);
    if (err < 0)
        abort();
}

void Carrot::VulkanRenderer::initImgui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForVulkan(driver.getWindow().get(), true);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = driver.getInstance();
    init_info.PhysicalDevice = driver.getPhysicalDevice();
    init_info.Device = driver.getLogicalDevice();
    init_info.QueueFamily = driver.getQueueFamilies().graphicsFamily.value();
    init_info.Queue = driver.getGraphicsQueue();
    init_info.PipelineCache = nullptr;
    init_info.DescriptorPool = *imguiDescriptorPool;
    init_info.Allocator = nullptr;

    SwapChainSupportDetails swapChainSupport = driver.querySwapChainSupport(driver.getPhysicalDevice());

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
    driver.performSingleTimeGraphicsCommands([&](vk::CommandBuffer& buffer) {
        ImGui_ImplVulkan_CreateFontsTexture(buffer);
    });
}

void Carrot::VulkanRenderer::createSkyboxResources() {
    skyboxImages.resize(getSwapchainImageCount());
    skyboxImageViews.resize(getSwapchainImageCount());
    for (int i = 0; i < getSwapchainImageCount(); ++i) {
        skyboxImages[i] = move(make_unique<Image>(driver,
                                              vk::Extent3D{driver.getSwapchainExtent().width, driver.getSwapchainExtent().height, 1},
                                              vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
                                              vk::Format::eR8G8B8A8Unorm));

        auto view = skyboxImages[i]->createImageView();
        skyboxImageViews[i] = std::move(view);

        skyboxImages[i]->transitionLayout(vk::Format::eR8G8B8A8Unorm, vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal);
    }
}

void Carrot::VulkanRenderer::createUIImages() {
    uiImages.resize(getSwapchainImageCount());
    uiImageViews.resize(getSwapchainImageCount());
    for (int i = 0; i < getSwapchainImageCount(); ++i) {
        uiImages[i] = move(make_unique<Image>(driver,
                                              vk::Extent3D{driver.getSwapchainExtent().width, driver.getSwapchainExtent().height, 1},
                                              vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
                                              vk::Format::eR8G8B8A8Unorm));

        auto view = uiImages[i]->createImageView();
        uiImageViews[i] = std::move(view);
    }
}

void Carrot::VulkanRenderer::onSwapchainImageCountChange(size_t newCount) {
    raytracer->onSwapchainImageCountChange(newCount);
    gBuffer->onSwapchainImageCountChange(newCount);
    for(const auto& [name, pipe]: pipelines) {
        pipe->onSwapchainImageCountChange(newCount);
    }
}

void Carrot::VulkanRenderer::onSwapchainSizeChange(int newWidth, int newHeight) {
    createUIImages();
    createSkyboxResources();
    raytracer->onSwapchainSizeChange(newWidth, newHeight);
    gBuffer->onSwapchainSizeChange(newWidth, newHeight);
    createRenderPasses();
    createFramebuffers();

    for(const auto& [name, pipe]: pipelines) {
        pipe->onSwapchainSizeChange(newWidth, newHeight);
    }
}
