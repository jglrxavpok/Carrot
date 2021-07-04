//
// Created by jglrxavpok on 13/03/2021.
//

#include "VulkanRenderer.h"
#include "GBuffer.h"
#include "engine/render/raytracing/ASBuilder.h"
#include "engine/render/raytracing/RayTracer.h"
#include "engine/utils/Assert.h"
#include "imgui.h"

Carrot::VulkanRenderer::VulkanRenderer(VulkanDriver& driver): driver(driver) {
    createRayTracer();
    createUIResources();
    createSkyboxResources();
    createGBuffer();
    createRenderPasses();
    createFramebuffers();

    initImGui();
    gBuffer->loadResolvePipeline();
}

shared_ptr<Carrot::Pipeline> Carrot::VulkanRenderer::getOrCreatePipeline(const vk::RenderPass& renderPass, const string& name) {
    auto key = std::make_pair(renderPass, name);
    auto it = pipelines.find(key);
    if(it == pipelines.end()) {
        pipelines[key] = make_shared<Pipeline>(driver, renderPass, "resources/pipelines/"+name+".json");
    }
    return pipelines[key];
}

unique_ptr<Carrot::Render::Texture>& Carrot::VulkanRenderer::getOrCreateTexture(const string& textureName) {
    auto it = textures.find(textureName);
    if(it == textures.end()) {
        auto texture = std::make_unique<Carrot::Render::Texture>(driver, "resources/textures/" + textureName);
        textures[textureName] = move(texture);
    }
    return textures[textureName];
}

void Carrot::VulkanRenderer::recreateDescriptorPools(size_t frameCount) {
    for(const auto& [name, pipeline] : pipelines) {
        pipeline->recreateDescriptorPool(frameCount);
    }
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
                driver.getSwapchainTextures()[i]->getView(),
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
                uiTextures[i]->getView()
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
                skyboxTextures[i]->getView()
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

void Carrot::VulkanRenderer::initImGui() {
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
    imguiInitInfo = {};
    imguiInitInfo.Instance = driver.getInstance();
    imguiInitInfo.PhysicalDevice = driver.getPhysicalDevice();
    imguiInitInfo.Device = driver.getLogicalDevice();
    imguiInitInfo.QueueFamily = driver.getQueueFamilies().graphicsFamily.value();
    imguiInitInfo.Queue = driver.getGraphicsQueue();
    imguiInitInfo.PipelineCache = nullptr;
    imguiInitInfo.DescriptorPool = *imguiDescriptorPool;
    imguiInitInfo.Allocator = nullptr;
}

void Carrot::VulkanRenderer::initImGuiPass(const vk::RenderPass& renderPass) {
    if(imguiIsInitialized) {
       ImGui_ImplVulkan_Shutdown();
    }

    SwapChainSupportDetails swapChainSupport = driver.querySwapChainSupport(driver.getPhysicalDevice());

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount +1;
    // maxImageCount == 0 means we can request any number of image
    if(swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        // ensure we don't ask for more images than the device will be able to provide
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    imguiInitInfo.MinImageCount = swapChainSupport.capabilities.minImageCount;
    imguiInitInfo.ImageCount = imageCount;
    imguiInitInfo.CheckVkResultFn = check_vk_result;
    ImGui_ImplVulkan_Init(&imguiInitInfo, renderPass);

    // Upload fonts
    driver.performSingleTimeGraphicsCommands([&](vk::CommandBuffer& buffer) {
        ImGui_ImplVulkan_CreateFontsTexture(buffer);
    });
}

void Carrot::VulkanRenderer::createSkyboxResources() {
    skyboxTextures.resize(getSwapchainImageCount());
    for (int i = 0; i < getSwapchainImageCount(); ++i) {
        skyboxTextures[i] = move(make_unique<Render::Texture>(driver,
                                              vk::Extent3D{driver.getSwapchainExtent().width, driver.getSwapchainExtent().height, 1},
                                              vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
                                              vk::Format::eR8G8B8A8Unorm));

        skyboxTextures[i]->transitionNow(vk::ImageLayout::eShaderReadOnlyOptimal);
    }
}

void Carrot::VulkanRenderer::createUIImages() {
    uiTextures.resize(getSwapchainImageCount());
    for (int i = 0; i < getSwapchainImageCount(); ++i) {
        uiTextures[i] = move(make_unique<Render::Texture>(driver,
                                              vk::Extent3D{driver.getSwapchainExtent().width, driver.getSwapchainExtent().height, 1},
                                              vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
                                              vk::Format::eR8G8B8A8Unorm));

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

// TODO: thread safety
void Carrot::VulkanRenderer::preFrame() {
    if(beforeFrameCommands.empty())
        return;
    driver.performSingleTimeGraphicsCommands([&](vk::CommandBuffer cmds) {
        for (const auto& action : beforeFrameCommands) {
            action(cmds);
        }
    });
    beforeFrameCommands.clear();
}
void Carrot::VulkanRenderer::postFrame() {
    if(afterFrameCommands.empty())
        return;
    driver.performSingleTimeGraphicsCommands([&](vk::CommandBuffer cmds) {
        for (const auto& action : afterFrameCommands) {
            action(cmds);
        }
    });
    afterFrameCommands.clear();
}

void Carrot::VulkanRenderer::beforeFrameCommand(const CommandBufferConsumer& command) {
    beforeFrameCommands.push_back(command);
}
void Carrot::VulkanRenderer::afterFrameCommand(const CommandBufferConsumer& command) {
    afterFrameCommands.push_back(command);
}

void Carrot::VulkanRenderer::bindSampler(Carrot::Pipeline& pipeline, const Carrot::Render::FrameData& frame, const vk::Sampler& samplerToBind, std::uint32_t setID, std::uint32_t bindingID) {
    runtimeAssert(setID == 0, "Engine does not support automatically reading sets beyond set 0... yet");
    auto& descriptorSet = pipeline.getDescriptorSets0()[frame.frameIndex];

    vk::DescriptorImageInfo samplerInfo {
            .sampler = samplerToBind,
    };

    vk::WriteDescriptorSet write {
            .dstSet = descriptorSet,
            .dstBinding = bindingID,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eSampler,
            .pImageInfo = &samplerInfo,
    };
    driver.getLogicalDevice().updateDescriptorSets(write, {});
}

void Carrot::VulkanRenderer::bindTexture(Carrot::Pipeline& pipeline, const Carrot::Render::FrameData& frame, const Carrot::Render::Texture& textureToBind, std::uint32_t setID, std::uint32_t bindingID, vk::ImageAspectFlags aspect) {
    bindTexture(pipeline, frame, textureToBind, setID, bindingID, driver.getLinearSampler(), aspect);
}

void Carrot::VulkanRenderer::bindTexture(Carrot::Pipeline& pipeline, const Carrot::Render::FrameData& frame, const Carrot::Render::Texture& textureToBind, std::uint32_t setID, std::uint32_t bindingID, vk::Sampler sampler, vk::ImageAspectFlags aspect) {
    runtimeAssert(setID == 0, "Engine does not support automatically reading sets beyond set 0... yet");
    auto& descriptorSet = pipeline.getDescriptorSets0()[frame.frameIndex];

    vk::DescriptorImageInfo imageInfo {
        .sampler = sampler,
        .imageView = textureToBind.getView(aspect),
        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
    };

    vk::DescriptorType descType = vk::DescriptorType::eSampledImage;

    if(sampler) {
        descType = vk::DescriptorType::eCombinedImageSampler;
    }
    vk::WriteDescriptorSet writeTexture {
            .dstSet = descriptorSet,
            .dstBinding = bindingID,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = descType,
            .pImageInfo = &imageInfo,
    };
    driver.getLogicalDevice().updateDescriptorSets(writeTexture, {});
}

Carrot::Render::Pass<Carrot::ImGuiPassData>& Carrot::VulkanRenderer::addImGuiPass(Carrot::Render::GraphBuilder& graph) {
    return graph.addPass<Carrot::ImGuiPassData>("imgui",
    [this](Render::GraphBuilder& graph, Render::Pass<Carrot::ImGuiPassData>& pass, Carrot::ImGuiPassData& data) {
        vk::ClearValue uiClear = vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,0.0f});
        data.output = graph.createRenderTarget(vk::Format::eR8G8B8A8Unorm, vk::Extent3D{driver.getSwapchainExtent().width, driver.getSwapchainExtent().height, 1}, vk::AttachmentLoadOp::eClear, uiClear);
    },
    [this](const Render::CompiledPass& pass, const Render::FrameData& frame, Carrot::ImGuiPassData& data, vk::CommandBuffer& cmds) {
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmds);
    },
    [this](Render::CompiledPass& pass, Carrot::ImGuiPassData& data)
    {
        initImGuiPass(pass.getRenderPass());
    });
}

void Carrot::VulkanRenderer::newFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
}
