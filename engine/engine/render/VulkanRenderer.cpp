//
// Created by jglrxavpok on 13/03/2021.
//

#include "VulkanRenderer.h"
#include "GBuffer.h"
#include "engine/render/raytracing/ASBuilder.h"
#include "engine/render/raytracing/RayTracer.h"
#include "engine/console/RuntimeOption.hpp"
#include "core/utils/Assert.h"
#include "imgui.h"
#include "engine/render/resources/BufferView.h"
#include "engine/render/MaterialSystem.h"
#include "engine/render/resources/Mesh.h"
#include "engine/render/Model.h"
#include "core/io/Logging.hpp"
#include "core/io/IO.h"
#include "engine/render/DrawData.h"

static constexpr std::size_t SingleFrameAllocatorSize = 512 * 1024 * 1024; // 512Mb per frame-in-flight
static Carrot::RuntimeOption DebugRenderPacket("Debug Render Packets", false);

Carrot::VulkanRenderer::VulkanRenderer(VulkanDriver& driver, Configuration config): driver(driver), config(config),

                                                                                    singleFrameAllocator(SingleFrameAllocatorSize)
{
    ZoneScoped;
    fullscreenQuad = std::make_unique<Mesh>(driver,
                                       std::vector<ScreenSpaceVertex>{
                                               { { -1, -1} },
                                               { { 1, -1} },
                                               { { 1, 1} },
                                               { { -1, 1} },
                                       },
                                       std::vector<uint32_t>{
                                               2,1,0,
                                               3,2,0,
                                       });

    createCameraSetResources();
    createRayTracer();
    createUIResources();
    createGBuffer();

    initImGui();
}

void Carrot::VulkanRenderer::lateInit() {
    unitSphereModel = getOrCreateModel("resources/models/simple_sphere.obj");
    unitCubeModel = getOrCreateModel("resources/models/simple_cube.obj");
    wireframeGBufferPipeline = getOrCreatePipeline("gBufferWireframe");
    gBufferPipeline = getOrCreatePipeline("gBuffer");
    whiteMaterial = getMaterialSystem().createMaterialHandle();
    whiteMaterial->diffuseTexture = getMaterialSystem().getWhiteTexture();
}

std::shared_ptr<Carrot::Pipeline> Carrot::VulkanRenderer::getOrCreateRenderPassSpecificPipeline(const std::string& name, const vk::RenderPass& renderPass) {
    return getOrCreatePipeline(name, reinterpret_cast<std::uint64_t>((void*) &renderPass));
}

std::shared_ptr<Carrot::Pipeline> Carrot::VulkanRenderer::getOrCreatePipeline(const std::string& name, std::uint64_t instanceOffset) {
    auto key = std::make_pair(name, instanceOffset);
    auto it = pipelines.find(key);
    if(it == pipelines.end()) {
        pipelines[key] = std::make_shared<Pipeline>(driver, "resources/pipelines/"+name+".json");
    }
    return pipelines[key];
}

std::shared_ptr<Carrot::Render::Texture> Carrot::VulkanRenderer::getOrCreateTextureFullPath(const std::string& textureName) {
    auto it = textures.find(textureName);
    if(it == textures.end()) {
        Carrot::IO::Resource from;
        try {
            from = textureName;
        } catch(std::runtime_error& e) {
            Carrot::Log::error("Could not open texture '%s'", textureName.c_str());
            // in case file could not be opened
            from = "resources/textures/default.png";
        }
        auto texture = std::make_unique<Carrot::Render::Texture>(driver, std::move(from));
        textures[textureName] = move(texture);
    }
    return textures[textureName];
}

std::shared_ptr<Carrot::Render::Texture> Carrot::VulkanRenderer::getOrCreateTextureFromResource(const Carrot::IO::Resource& from) {
    if(from.isFile()) {
        const auto& textureName = from.getName();
        auto it = textures.find(textureName);
        if(it == textures.end()) {
            auto texture = std::make_unique<Carrot::Render::Texture>(driver, std::move(from));
            textures[textureName] = move(texture);
        }
        return textures[textureName];
    } else {
        auto texture = std::make_shared<Carrot::Render::Texture>(driver, std::move(from));
        return texture;
    }
}

std::shared_ptr<Carrot::Render::Texture> Carrot::VulkanRenderer::getOrCreateTexture(const std::string& textureName) {
    return getOrCreateTextureFullPath("resources/textures/" + textureName);
}

std::shared_ptr<Carrot::Model> Carrot::VulkanRenderer::getOrCreateModel(const std::string& modelPath) {
    auto it = models.find(modelPath);
    if(it == models.end()) {
        Carrot::IO::Resource from;
        try {
            from = modelPath;
        } catch(std::runtime_error& e) {
            // in case file could not be opened
            from = "resources/models/simple_cube.obj";
        }
        auto model = std::make_unique<Carrot::Model>(getEngine(), std::move(from));
        models[modelPath] = move(model);
    }
    return models[modelPath];
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
    gBuffer = std::make_unique<GBuffer>(*this, *raytracer);
}

void Carrot::VulkanRenderer::createRayTracer() {
    raytracer = std::make_unique<RayTracer>(*this);
    asBuilder = std::make_unique<ASBuilder>(*this);
}

static void imguiCheckVkResult(VkResult err) {
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
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigWindowsResizeFromEdges = true;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForVulkan(driver.getWindow().getGLFWPointer(), true);
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

    std::uint32_t imageCount = swapChainSupport.capabilities.minImageCount +1;
    // maxImageCount == 0 means we can request any number of image
    if(swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        // ensure we don't ask for more images than the device will be able to provide
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    imguiInitInfo.MinImageCount = swapChainSupport.capabilities.minImageCount;
    imguiInitInfo.ImageCount = imageCount;
    imguiInitInfo.CheckVkResultFn = imguiCheckVkResult;
    ImGui_ImplVulkan_Init(&imguiInitInfo, renderPass);

    // Upload fonts
    driver.performSingleTimeGraphicsCommands([&](vk::CommandBuffer& buffer) {
        ImGui_ImplVulkan_CreateFontsTexture(buffer);
    });
}

void Carrot::VulkanRenderer::onSwapchainImageCountChange(std::size_t newCount) {
    raytracer->onSwapchainImageCountChange(newCount);
    gBuffer->onSwapchainImageCountChange(newCount);
    materialSystem.onSwapchainImageCountChange(newCount);
    lighting.onSwapchainImageCountChange(newCount);
    for(const auto& [name, pipe]: pipelines) {
        pipe->onSwapchainImageCountChange(newCount);
    }
}

void Carrot::VulkanRenderer::onSwapchainSizeChange(int newWidth, int newHeight) {
    raytracer->onSwapchainSizeChange(newWidth, newHeight);
    gBuffer->onSwapchainSizeChange(newWidth, newHeight);
    materialSystem.onSwapchainSizeChange(newWidth, newHeight);
    lighting.onSwapchainSizeChange(newWidth, newHeight);
    for(const auto& [name, pipe]: pipelines) {
        pipe->onSwapchainSizeChange(newWidth, newHeight);
    }
}

// TODO: thread safety
void Carrot::VulkanRenderer::preFrame() {
    for(auto& [name, pipeline] : pipelines) {
        pipeline->checkForReloadableShaders();
    }

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
    if(!afterFrameCommands.empty()) {
        driver.performSingleTimeGraphicsCommands([&](vk::CommandBuffer cmds) {
            for (const auto& action : afterFrameCommands) {
                action(cmds);
            }
        });
        afterFrameCommands.clear();
    }
    pushConstants.clear();
}

void Carrot::VulkanRenderer::beforeFrameCommand(const CommandBufferConsumer& command) {
    beforeFrameCommands.push_back(command);
}
void Carrot::VulkanRenderer::afterFrameCommand(const CommandBufferConsumer& command) {
    afterFrameCommands.push_back(command);
}

void Carrot::VulkanRenderer::bindSampler(Carrot::Pipeline& pipeline, const Carrot::Render::Context& frame, const vk::Sampler& samplerToBind, std::uint32_t setID, std::uint32_t bindingID) {
    verify(setID == 0, "Engine does not support automatically reading sets beyond set 0... yet");
    if(boundSamplers[{pipeline.getPipelineLayout(), frame.swapchainIndex, setID, bindingID}] == samplerToBind) {
        return;
    }
    boundSamplers[{pipeline.getPipelineLayout(), frame.swapchainIndex, setID, bindingID}] = samplerToBind;
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

void Carrot::VulkanRenderer::bindTexture(Carrot::Pipeline& pipeline, const Carrot::Render::Context& frame, const Carrot::Render::Texture& textureToBind, std::uint32_t setID, std::uint32_t bindingID, vk::ImageAspectFlags aspect, vk::ImageViewType viewType, std::uint32_t arrayIndex) {
    bindTexture(pipeline, frame, textureToBind, setID, bindingID, driver.getLinearSampler(), aspect, viewType, arrayIndex);
}

void Carrot::VulkanRenderer::bindTexture(Carrot::Pipeline& pipeline, const Carrot::Render::Context& frame, const Carrot::Render::Texture& textureToBind, std::uint32_t setID, std::uint32_t bindingID, vk::Sampler sampler, vk::ImageAspectFlags aspect, vk::ImageViewType viewType, std::uint32_t arrayIndex) {
    ZoneScoped;
    verify(setID == 0, "Engine does not support automatically reading sets beyond set 0... yet");
    if(boundTextures[{pipeline.getPipelineLayout(), frame.swapchainIndex, setID, bindingID}] == textureToBind.getVulkanImage()) {
        return;
    }
    boundTextures[{pipeline.getPipelineLayout(), frame.swapchainIndex, setID, bindingID}] = textureToBind.getVulkanImage();
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
    std::array<vk::WriteDescriptorSet, 1> writeTexture {
            vk::WriteDescriptorSet {
                .dstSet = descriptorSet,
                .dstBinding = bindingID,
                .dstArrayElement = arrayIndex,
                .descriptorCount = 1,
                .descriptorType = descType,
                .pImageInfo = &imageInfo,
            }
    };
    {
        ZoneScopedN("driver.getLogicalDevice().updateDescriptorSets");
        driver.getLogicalDevice().updateDescriptorSets(writeTexture, {});
    }
}

Carrot::Render::Pass<Carrot::Render::PassData::ImGui>& Carrot::VulkanRenderer::addImGuiPass(Carrot::Render::GraphBuilder& graph) {
    return graph.addPass<Carrot::Render::PassData::ImGui>("imgui",
    [this](Render::GraphBuilder& graph, Render::Pass<Carrot::Render::PassData::ImGui>& pass, Carrot::Render::PassData::ImGui& data) {
        vk::ClearValue uiClear = vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,0.0f});
        auto& windowSize = getVulkanDriver().getWindowFramebufferExtent();
        data.output = graph.createRenderTarget(vk::Format::eR8G8B8A8Unorm, windowSize, vk::AttachmentLoadOp::eClear, uiClear);
    },
    [this](const Render::CompiledPass& pass, const Render::Context& frame, Carrot::Render::PassData::ImGui& data, vk::CommandBuffer& cmds) {
        ZoneScopedN("CPU RenderGraph ImGui");
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

    std::size_t previousCapacity = renderPackets.capacity();
    renderPackets.clear();
    renderPackets.reserve(previousCapacity);
}

void Carrot::VulkanRenderer::bindCameraSet(vk::PipelineBindPoint bindPoint, const vk::PipelineLayout& pipelineLayout, const Render::Context& data, vk::CommandBuffer& cmds, std::uint32_t setID) {
    auto cameraSet = data.getCameraDescriptorSet();
    cmds.bindDescriptorSets(bindPoint, pipelineLayout, setID, {cameraSet}, {});
}

void Carrot::VulkanRenderer::blit(Carrot::Render::Texture& source, Carrot::Render::Texture& destination, vk::CommandBuffer& cmds, vk::Offset3D srcOffset, vk::Offset3D dstOffset) {
    assert(source.getImage().getLayerCount() == destination.getImage().getLayerCount());

    source.transitionInline(cmds, vk::ImageLayout::eTransferSrcOptimal);
    destination.transitionInline(cmds, vk::ImageLayout::eTransferDstOptimal);

    vk::ImageBlit blitInfo;
    blitInfo.dstSubresource.aspectMask = blitInfo.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    blitInfo.dstSubresource.layerCount = blitInfo.srcSubresource.layerCount = source.getImage().getLayerCount();
    blitInfo.dstOffsets[0] = dstOffset;
    blitInfo.dstOffsets[1] = vk::Offset3D{ (int32_t)(dstOffset.x + destination.getSize().width), (int32_t)(dstOffset.y + destination.getSize().height), (int32_t)(dstOffset.z + destination.getSize().depth) };
    blitInfo.srcOffsets[1] = vk::Offset3D{ (int32_t)(srcOffset.x + source.getSize().width), (int32_t)(srcOffset.y + source.getSize().height), (int32_t)(srcOffset.z + source.getSize().depth) };

    cmds.blitImage(source.getVulkanImage(), vk::ImageLayout::eTransferSrcOptimal, destination.getVulkanImage(), vk::ImageLayout::eTransferDstOptimal, blitInfo, vk::Filter::eNearest);
}

void Carrot::VulkanRenderer::beginFrame(const Carrot::Render::Context& renderContext) {
    materialSystem.beginFrame(renderContext);
    lighting.beginFrame(renderContext);
    singleFrameAllocator.newFrame(renderContext.swapchainIndex);

#ifdef IS_DEBUG_BUILD
    if(glfwGetKey(driver.getWindow().getGLFWPointer(), GLFW_KEY_F5) == GLFW_PRESS) {
        driver.breakOnNextVulkanError();
    }
#endif
}

void Carrot::VulkanRenderer::endFrame(const Carrot::Render::Context& renderContext) {
    sortRenderPackets();
    mergeRenderPackets();
}

void Carrot::VulkanRenderer::onFrame(const Carrot::Render::Context& renderContext) {

}

Carrot::Engine& Carrot::VulkanRenderer::getEngine() {
    return driver.getEngine();
}

void Carrot::VulkanRenderer::createCameraSetResources() {
    vk::DescriptorSetLayoutBinding cameraBinding {
            .binding = 0,
            .descriptorType = vk::DescriptorType::eUniformBuffer,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eAllGraphics | vk::ShaderStageFlagBits::eRaygenKHR,
    };
    cameraDescriptorSetLayout = getVulkanDriver().getLogicalDevice().createDescriptorSetLayoutUnique(vk::DescriptorSetLayoutCreateInfo {
            .bindingCount = 1,
            .pBindings = &cameraBinding,
    });

    vk::DescriptorPoolSize poolSize {
            .type = vk::DescriptorType::eUniformBuffer,
            .descriptorCount = 1,
    };
    cameraDescriptorPool = getVulkanDriver().getLogicalDevice().createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo {
            .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            .maxSets = static_cast<uint32_t>(getSwapchainImageCount()) * MaxCameras,
            .poolSizeCount = 1,
            .pPoolSizes = &poolSize,
    });
}

std::vector<vk::DescriptorSet> Carrot::VulkanRenderer::createDescriptorSetForCamera(const std::vector<Carrot::BufferView>& uniformBuffers) {
    std::vector<vk::DescriptorSetLayout> layouts {getSwapchainImageCount(), *cameraDescriptorSetLayout};
    auto cameraDescriptorSets = getVulkanDriver().getLogicalDevice().allocateDescriptorSets(vk::DescriptorSetAllocateInfo {
            .descriptorPool = *cameraDescriptorPool,
            .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
            .pSetLayouts = layouts.data(),
    });

    assert(cameraDescriptorSets.size() == uniformBuffers.size());
    for (int i = 0; i < cameraDescriptorSets.size(); i++) {
        vk::DescriptorBufferInfo cameraBuffer = uniformBuffers[i].asBufferInfo();
        vk::WriteDescriptorSet write {
                .dstSet = cameraDescriptorSets[i],
                .dstBinding = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eUniformBuffer,
                .pBufferInfo = &cameraBuffer,
        };

        getVulkanDriver().getLogicalDevice().updateDescriptorSets(write, {});
    }
    return cameraDescriptorSets;
}

void Carrot::VulkanRenderer::destroyCameraDescriptorSets(const std::vector<vk::DescriptorSet>& sets) {
    if(sets.empty())
        return;
    getVulkanDriver().getLogicalDevice().freeDescriptorSets(*cameraDescriptorPool, sets);
}

const vk::DescriptorSetLayout& Carrot::VulkanRenderer::getCameraDescriptorSetLayout() const {
    return *cameraDescriptorSetLayout;
}

void Carrot::VulkanRenderer::fullscreenBlit(const vk::RenderPass& pass, const Carrot::Render::Context& frame, Carrot::Render::Texture& textureToBlit, Carrot::Render::Texture& targetTexture, vk::CommandBuffer& cmds) {
    auto pipeline = getOrCreateRenderPassSpecificPipeline("blit", pass);
    vk::DescriptorImageInfo imageInfo {
            .sampler = getVulkanDriver().getLinearSampler(),
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
    getVulkanDriver().getLogicalDevice().updateDescriptorSets(writeImage, {});

    pipeline->bind(pass, frame, cmds);
    getFullscreenQuad().bind(cmds);
    getFullscreenQuad().draw(cmds);
}

Carrot::Render::Texture::Ref Carrot::VulkanRenderer::getDefaultImage() {
    return getOrCreateTexture("default.png");
}

void Carrot::VulkanRenderer::recordOpaqueGBufferPass(vk::RenderPass pass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) {
    Carrot::Render::Viewport* viewport = &renderContext.viewport;
    verify(viewport, "Viewport cannot be null");

    auto packets = getRenderPackets(viewport, Carrot::Render::PassEnum::OpaqueGBuffer);
    for(const auto& p : packets) {
        p.record(pass, renderContext, commands);
    }
}

void Carrot::VulkanRenderer::recordTransparentGBufferPass(vk::RenderPass pass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) {
    Carrot::Render::Viewport* viewport = &renderContext.viewport;
    verify(viewport, "Viewport cannot be null");

    auto packets = getRenderPackets(viewport, Carrot::Render::PassEnum::TransparentGBuffer);
    for(const auto& p : packets) {
        p.record(pass, renderContext, commands);
    }
}

std::span<const Carrot::Render::Packet> Carrot::VulkanRenderer::getRenderPackets(Carrot::Render::Viewport* viewport, Carrot::Render::PassEnum pass) const {
    auto predicate = [&](const auto& packet) {
        return packet.viewport == viewport && packet.pass == pass;
    };
    auto first = std::find_if(preparedRenderPackets.begin(), preparedRenderPackets.end(), predicate);
    if(first == preparedRenderPackets.end())
        return {};
    std::size_t startIndex = std::distance(preparedRenderPackets.begin(), first);
    std::size_t endIndexInclusive = startIndex;
    for (std::size_t index = startIndex; index < preparedRenderPackets.size(); ++index) {
        if(!predicate(preparedRenderPackets[index])) {
            break;
        }

        endIndexInclusive = index;
    }

    return std::span<const Render::Packet> { &preparedRenderPackets[startIndex], static_cast<std::size_t>(endIndexInclusive - startIndex + 1) };
}

void Carrot::VulkanRenderer::sortRenderPackets() {
    // sort by viewport, pass, then pipeline, then mesh
    std::sort(renderPackets.begin(), renderPackets.end(), [&](const Render::Packet& a, const Render::Packet& b) {
        if(a.viewport != b.viewport) {
            return a.viewport < b.viewport;
        }

        if(a.pass != b.pass) {
            return a.pass < b.pass;
        }

        if(a.pipeline != b.pipeline) {
            return a.pipeline < b.pipeline;
        }

        return a.vertexBuffer.getVulkanBuffer() < b.vertexBuffer.getVulkanBuffer();
    });
}

void Carrot::VulkanRenderer::mergeRenderPackets() {
    std::size_t previousCapacity = preparedRenderPackets.capacity();
    preparedRenderPackets.clear();
    preparedRenderPackets.reserve(previousCapacity);

    if(renderPackets.empty()) {
        return;
    }

    static bool doMerge = true;
    float mergeTime = 0.0f;

    {
        Carrot::Profiling::ScopedTimer mergeTimer("Merge timer");
        Render::Packet mergeResult = renderPackets[0];
        for(std::size_t i = 1; i < renderPackets.size(); i++) {
            const Render::Packet& currentPacket = renderPackets[i];
            if(!doMerge || !mergeResult.merge(currentPacket)) {
                preparedRenderPackets.push_back(mergeResult);
                mergeResult = currentPacket;
            }
        }

        preparedRenderPackets.push_back(mergeResult);

        mergeTime = mergeTimer.getTime();
    }

    if(DebugRenderPacket) {
        bool open = true;
        if(ImGui::Begin("Debug Render Packets", &open)) {
            if(!open) {
                DebugRenderPacket.setValue(false);
            }

            ImGui::Checkbox("Merge Render Packets with instancing", &doMerge);
            ImGui::Text("Total render packets count: %llu", renderPackets.size());
            ImGui::Text("Merged render packets count: %llu", preparedRenderPackets.size());
            float ratio = static_cast<float>(preparedRenderPackets.size()) / static_cast<float>(renderPackets.size());
            ImGui::Separator();
            ImGui::Text("Time for Render Packet merge: %0.3f ms", mergeTime*1000.0f);
            ImGui::Text("Draw call reduction: %0.1f%%", (1.0f - ratio)*100);
            ImGui::Text("Instance buffer size this frame: %s", Carrot::IO::getHumanReadableFileSize(singleFrameAllocator.getAllocatedSizeThisFrame()).c_str());
            ImGui::Text("Instance buffer size total: %s", Carrot::IO::getHumanReadableFileSize(singleFrameAllocator.getAllocatedSizeAllFrames()).c_str());
        }
        ImGui::End();
    }
}

void Carrot::VulkanRenderer::renderWireframeSphere(const Carrot::Render::Context& renderContext, const glm::vec3& position, float radius, const glm::vec4& color, const Carrot::UUID& objectID) {
    Carrot::Render::Packet packet(Carrot::Render::PassEnum::OpaqueGBuffer);
    Carrot::DrawData data;
    data.materialIndex = whiteMaterial->getSlot();

    packet.viewport = &renderContext.viewport;
    packet.useMesh(*unitSphereModel->getStaticMeshes()[0]); // TODO: find a better way to load individual meshes
    packet.pipeline = wireframeGBufferPipeline;

    Carrot::Render::Packet::PushConstant& pushConstant = packet.addPushConstant();
    pushConstant.id = "drawDataPush";
    pushConstant.stages = vk::ShaderStageFlagBits::eFragment;

    pushConstant.setData(data);

    Carrot::InstanceData instance;
    instance.uuid = objectID;
    instance.color = color;
    instance.transform = glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), glm::vec3(radius) * 2.0f);
    packet.useInstance(instance);
    render(packet);
}

void Carrot::VulkanRenderer::renderWireframeCuboid(const Carrot::Render::Context& renderContext, const glm::vec3& position, const glm::vec3& halfExtents, const glm::vec4& color, const Carrot::UUID& objectID) {
    Carrot::Render::Packet packet(Carrot::Render::PassEnum::OpaqueGBuffer);
    Carrot::DrawData data;
    data.materialIndex = whiteMaterial->getSlot();

    packet.viewport = &renderContext.viewport;
    packet.useMesh(*unitCubeModel->getStaticMeshes()[0]); // TODO: find a better way to load individual meshes
    packet.pipeline = wireframeGBufferPipeline;

    Carrot::Render::Packet::PushConstant& pushConstant = packet.addPushConstant();
    pushConstant.id = "drawDataPush";
    pushConstant.stages = vk::ShaderStageFlagBits::eFragment;

    pushConstant.setData(data);

    Carrot::InstanceData instance;
    instance.uuid = objectID;
    instance.color = color;
    instance.transform = glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), glm::vec3(halfExtents) * 2.0f);
    packet.useInstance(instance);
    render(packet);
}

void Carrot::VulkanRenderer::render(const Render::Packet& packet) {
    verify(packet.pipeline, "Pipeline must not be null");
    verify(packet.pass != Render::PassEnum::Undefined, "Render pass must be defined");
    verify(packet.vertexBuffer, "Vertex buffer must not be null");
    verify(packet.indexBuffer, "Index buffer must not be null");
    verify(packet.viewport, "Viewport must not be null");
    renderPackets.push_back(packet);
}

Carrot::BufferView Carrot::VulkanRenderer::getInstanceBuffer(std::size_t bytes) {
    return singleFrameAllocator.getInstanceBuffer(bytes);
}
