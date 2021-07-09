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
    createGBuffer();

    initImGui();
}

shared_ptr<Carrot::Pipeline> Carrot::VulkanRenderer::getOrCreatePipeline(const string& name) {
    auto key = name;
    auto it = pipelines.find(key);
    if(it == pipelines.end()) {
        pipelines[key] = make_shared<Pipeline>(driver, "resources/pipelines/"+name+".json");
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

void Carrot::VulkanRenderer::onSwapchainImageCountChange(size_t newCount) {
    raytracer->onSwapchainImageCountChange(newCount);
    gBuffer->onSwapchainImageCountChange(newCount);
    for(const auto& [name, pipe]: pipelines) {
        pipe->onSwapchainImageCountChange(newCount);
    }
}

void Carrot::VulkanRenderer::onSwapchainSizeChange(int newWidth, int newHeight) {
    raytracer->onSwapchainSizeChange(newWidth, newHeight);
    gBuffer->onSwapchainSizeChange(newWidth, newHeight);

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

void Carrot::VulkanRenderer::bindSampler(Carrot::Pipeline& pipeline, const Carrot::Render::Context& frame, const vk::Sampler& samplerToBind, std::uint32_t setID, std::uint32_t bindingID) {
    runtimeAssert(setID == 0, "Engine does not support automatically reading sets beyond set 0... yet");
    auto& descriptorSet = pipeline.getDescriptorSets0()[frame.swapchainIndex];

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

void Carrot::VulkanRenderer::bindTexture(Carrot::Pipeline& pipeline, const Carrot::Render::Context& frame, const Carrot::Render::Texture& textureToBind, std::uint32_t setID, std::uint32_t bindingID, vk::ImageAspectFlags aspect, vk::ImageViewType viewType) {
    bindTexture(pipeline, frame, textureToBind, setID, bindingID, driver.getLinearSampler(), aspect, viewType);
}

void Carrot::VulkanRenderer::bindTexture(Carrot::Pipeline& pipeline, const Carrot::Render::Context& frame, const Carrot::Render::Texture& textureToBind, std::uint32_t setID, std::uint32_t bindingID, vk::Sampler sampler, vk::ImageAspectFlags aspect, vk::ImageViewType viewType) {
    runtimeAssert(setID == 0, "Engine does not support automatically reading sets beyond set 0... yet");
    auto& descriptorSet = pipeline.getDescriptorSets0()[frame.swapchainIndex];

    vk::DescriptorImageInfo imageInfo {
        .sampler = sampler,
        .imageView = textureToBind.getView(textureToBind.getImage().getFormat(), aspect, viewType),
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

Carrot::Render::Pass<Carrot::Render::PassData::ImGui>& Carrot::VulkanRenderer::addImGuiPass(Carrot::Render::GraphBuilder& graph) {
    return graph.addPass<Carrot::Render::PassData::ImGui>("imgui",
    [this](Render::GraphBuilder& graph, Render::Pass<Carrot::Render::PassData::ImGui>& pass, Carrot::Render::PassData::ImGui& data) {
        vk::ClearValue uiClear = vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,0.0f});
        data.output = graph.createRenderTarget(vk::Format::eR8G8B8A8Unorm, {}, vk::AttachmentLoadOp::eClear, uiClear);
    },
    [this](const Render::CompiledPass& pass, const Render::Context& frame, Carrot::Render::PassData::ImGui& data, vk::CommandBuffer& cmds) {
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmds);
    },
    [this](Render::CompiledPass& pass, Carrot::Render::PassData::ImGui& data)
    {
        initImGuiPass(pass.getRenderPass());
    });
}

void Carrot::VulkanRenderer::newFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
}
