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
#include "engine/render/resources/SingleMesh.h"
#include "engine/render/Model.h"
#include "core/io/Logging.hpp"
#include "core/io/IO.h"
#include "engine/render/DebugBufferObject.h"
#include "engine/render/GBufferDrawData.h"
#include "engine/render/resources/Buffer.h"
#include "engine/render/resources/Font.h"
#include "engine/render/resources/ResourceAllocator.h"
#include "engine/math/Transform.h"
#include <execution>
#include <robin_hood.h>
#include <core/math/BasicFunctions.h>

static constexpr std::size_t SingleFrameAllocatorSize = 512 * 1024 * 1024; // 512Mb per frame-in-flight
static Carrot::RuntimeOption DebugRenderPacket("Debug Render Packets", false);
static Carrot::RuntimeOption ShowGBuffer("Engine/Show GBuffer", false);

static thread_local Carrot::VulkanRenderer::ThreadPackets* threadLocalRenderPackets = nullptr;
static thread_local Carrot::Render::PacketContainer* threadLocalPacketStorage = nullptr;


struct ForwardFrameInfo {
    std::uint32_t frameCount;
    std::uint32_t frameWidth;
    std::uint32_t frameHeight;

    bool hasTLAS;
};

Carrot::VulkanRenderer::VulkanRenderer(VulkanDriver& driver, Configuration config): driver(driver), config(config),

                                                                                    singleFrameAllocator(SingleFrameAllocatorSize)
{
    ZoneScoped;

    nullBuffer = std::make_unique<Buffer>(driver,
                                          1,
                                          vk::BufferUsageFlagBits::eUniformBuffer
                                          | vk::BufferUsageFlagBits::eStorageBuffer,
                                          vk::MemoryPropertyFlagBits::eDeviceLocal
                                          );
    nullBufferInfo = nullBuffer->asBufferInfo();

    fullscreenQuad = std::make_unique<SingleMesh>(
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
    createViewportSetResources();
    createPerDrawSetResources();
    createDebugSetResources();
    createRayTracer();
    createUIResources();
    createGBuffer();
    createDefaultResources();

    initImGui();

    makeCurrentThreadRenderCapable();
}

void Carrot::VulkanRenderer::lateInit() {
    ZoneScoped;
    unitSphereModel = getOrCreateModel("resources/models/simple_sphere.obj");
    unitCubeModel = getOrCreateModel("resources/models/simple_cube.obj");
    unitCapsuleModel = getOrCreateModel("resources/models/simple_capsule.obj");
    wireframeGBufferPipeline = getOrCreatePipeline("gBufferWireframe");
    gBufferPipeline = getOrCreatePipeline("gBuffer");
    whiteMaterial = getMaterialSystem().createMaterialHandle();
    whiteMaterial->albedo = getMaterialSystem().getWhiteTexture();
}

std::shared_ptr<Carrot::Pipeline> Carrot::VulkanRenderer::getOrCreateRenderPassSpecificPipeline(const std::string& name, const vk::RenderPass& renderPass) {
    return getOrCreatePipeline(name, reinterpret_cast<std::uint64_t>((void*) &renderPass));
}

std::shared_ptr<Carrot::Pipeline> Carrot::VulkanRenderer::getOrCreatePipeline(const std::string& name, std::uint64_t instanceOffset) {
    ZoneScopedN("Loading pipeline");
    ZoneText(name.c_str(), name.size());
    auto key = std::make_pair(name, instanceOffset);
    return pipelines.getOrCompute(key, [&]() {
        return std::make_shared<Pipeline>(driver, "resources/pipelines/"+name+".json");
    });
}

std::shared_ptr<Carrot::Render::Texture> Carrot::VulkanRenderer::getOrCreateTextureFullPath(const std::string& textureName) {
    ZoneScopedN("Loading texture");
    ZoneText(textureName.c_str(), textureName.size());
    return textures.getOrCompute(textureName, [&]() {
        Carrot::IO::Resource from;
        try {
            from = textureName;
        } catch(std::runtime_error& e) {
            Carrot::Log::error("Could not open texture '%s'", textureName.c_str());
            // in case file could not be opened
            from = "resources/textures/default.png";
        }

        return std::make_shared<Carrot::Render::Texture>(driver, std::move(from));
    });
}

std::shared_ptr<Carrot::Render::Texture> Carrot::VulkanRenderer::getOrCreateTextureFromResource(const Carrot::IO::Resource& from) {
    if(from.isFile()) {
        const auto& textureName = from.getName();
        ZoneScopedN("Loading texture");
        ZoneText(textureName.c_str(), textureName.size());
        return textures.getOrCompute(textureName, [&]() {
            try {
                return std::make_shared<Carrot::Render::Texture>(driver, from);
            } catch (const std::exception& e) {
                Carrot::Log::error("Failed to load texture %s: %s", textureName.c_str(), e.what());
                return std::make_shared<Carrot::Render::Texture>(driver, "resources/textures/default.png");
            }
        });
    } else {
        auto texture = std::make_shared<Carrot::Render::Texture>(driver, from);
        return texture;
    }
}

std::shared_ptr<Carrot::Render::Texture> Carrot::VulkanRenderer::getOrCreateTexture(const std::string& textureName) {
    return getOrCreateTextureFullPath("resources/textures/" + textureName);
}

std::shared_ptr<Carrot::Render::Font> Carrot::VulkanRenderer::getOrCreateFront(const Carrot::IO::Resource& from) {
    if(from.isFile()) {
        const auto& fontName = from.getName();
        ZoneScopedN("Loading font");
        ZoneText(fontName.c_str(), fontName.size());
        return fonts.getOrCompute(fontName, [&]() {
            return std::make_shared<Carrot::Render::Font>(*this, from);
        });
    } else {
        auto font = std::make_shared<Carrot::Render::Font>(*this, from);
        return font;
    }
}

std::shared_ptr<Carrot::Model> Carrot::VulkanRenderer::getOrCreateModel(const std::string& modelPath) {
    ZoneScopedN("Loading model");
    ZoneText(modelPath.c_str(), modelPath.size());
    return models.getOrCompute(modelPath, [&]() {

        Carrot::IO::Resource from;
        try {
            from = modelPath;
        } catch(std::runtime_error& e) {
            Carrot::Log::error("Failed to load model %s", modelPath.c_str());
            // in case file could not be opened
            from = "resources/models/simple_cube.obj";
        }
        return std::make_shared<Carrot::Model>(getEngine(), std::move(from));
    });
}

void Carrot::VulkanRenderer::recreateDescriptorPools(size_t frameCount) {
    for(const auto& [nameHash, pipelinePtrPtr] : pipelines.snapshot()) {
        (*pipelinePtrPtr)->recreateDescriptorPool(frameCount);
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
    io.ConfigFlags |= GetConfiguration().runInVR ? 0 : ImGuiConfigFlags_ViewportsEnable;

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
    imguiInitInfo.Queue = driver.getGraphicsQueue().getQueueUnsafe();
    imguiInitInfo.PipelineCache = nullptr;
    imguiInitInfo.DescriptorPool = *imguiDescriptorPool;
    imguiInitInfo.Allocator = nullptr;

    // TODO: move to dedicated style file
    auto font = io.Fonts->AddFontFromFileTTF(Carrot::toString(GetVFS().resolve(IO::VFS::Path("resources/fonts/Roboto-Medium.ttf")).u8string()).c_str(), 14.0f);
    io.Fonts->Build();
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
    boundTextures.clear();
    raytracer->onSwapchainImageCountChange(newCount);
    gBuffer->onSwapchainImageCountChange(newCount);
    materialSystem.onSwapchainImageCountChange(newCount);

    lighting.onSwapchainImageCountChange(newCount);
    forwardRenderingFrameInfo.clear();
    forwardRenderingFrameInfo.resize(getSwapchainImageCount());
    for (std::size_t j = 0; j < getSwapchainImageCount(); ++j) {
        forwardRenderingFrameInfo[j] = getEngine().getResourceAllocator().allocateDedicatedBuffer(
                sizeof(ForwardFrameInfo),
                vk::BufferUsageFlagBits::eUniformBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
        );
    }

    perDrawBuffers.resize(getSwapchainImageCount());
    perDrawOffsetBuffers.resize(getSwapchainImageCount());

    if(asBuilder) {
        asBuilder->onSwapchainImageCountChange(newCount);
    }
    for(const auto& [name, pipePtrPtr]: pipelines.snapshot()) {
        (*pipePtrPtr)->onSwapchainImageCountChange(newCount);
    }
}

void Carrot::VulkanRenderer::onSwapchainSizeChange(int newWidth, int newHeight) {
    boundTextures.clear();
    raytracer->onSwapchainSizeChange(newWidth, newHeight);
    gBuffer->onSwapchainSizeChange(newWidth, newHeight);
    materialSystem.onSwapchainSizeChange(newWidth, newHeight);
    lighting.onSwapchainSizeChange(newWidth, newHeight);
    if(asBuilder) {
        asBuilder->onSwapchainSizeChange(newWidth, newHeight);
    }
    for(const auto& [name, pipePtrPtr]: pipelines.snapshot()) {
        (*pipePtrPtr)->onSwapchainSizeChange(newWidth, newHeight);
    }
}

void Carrot::VulkanRenderer::preFrame(const Render::Context& renderContext) {
    updatePerDrawBuffers(renderContext);
    //updateIndirectDrawCommands(renderContext);

    // TODO: thread safety
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
    if(boundSamplers[{pipeline.getPipelineLayout(), frame.swapchainIndex, setID, bindingID}] == samplerToBind) {
        return;
    }
    boundSamplers[{pipeline.getPipelineLayout(), frame.swapchainIndex, setID, bindingID}] = samplerToBind;
    auto descriptorSet = pipeline.getDescriptorSets(frame, setID)[frame.swapchainIndex];

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

void Carrot::VulkanRenderer::bindAccelerationStructure(Carrot::Pipeline& pipeline, const Carrot::Render::Context& frame, Carrot::AccelerationStructure& as, std::uint32_t setID, std::uint32_t bindingID) {
    ZoneScoped;
    if(boundAS[{pipeline.getPipelineLayout(), frame.swapchainIndex, setID, bindingID}] == as.getVulkanAS()) {
        return;
    }
    boundAS[{pipeline.getPipelineLayout(), frame.swapchainIndex, setID, bindingID}] = as.getVulkanAS();
    auto descriptorSet = pipeline.getDescriptorSets(frame, setID)[frame.swapchainIndex];

    vk::WriteDescriptorSetAccelerationStructureKHR  asInfo {
            .accelerationStructureCount = 1,
            .pAccelerationStructures = &as.getVulkanAS(),
    };

    vk::DescriptorType descType = vk::DescriptorType::eAccelerationStructureKHR;

    std::array<vk::WriteDescriptorSet, 1> writeTexture {
            vk::WriteDescriptorSet {
                    .pNext = &asInfo,
                    .dstSet = descriptorSet,
                    .dstBinding = bindingID,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = descType,
            }
    };
    {
        ZoneScopedN("driver.getLogicalDevice().updateDescriptorSets");
        driver.getLogicalDevice().updateDescriptorSets(writeTexture, {});
    }
}

void Carrot::VulkanRenderer::bindUniformBuffer(Pipeline& pipeline, const Render::Context& frame, const BufferView& view, std::uint32_t setID, std::uint32_t bindingID) {
    ZoneScoped;
    if(boundBuffers[{pipeline.getPipelineLayout(), frame.swapchainIndex, setID, bindingID}] == view) {
        return;
    }
    boundBuffers[{pipeline.getPipelineLayout(), frame.swapchainIndex, setID, bindingID}] = view;
    auto descriptorSet = pipeline.getDescriptorSets(frame, setID)[frame.swapchainIndex];

    auto bufferInfo = view.asBufferInfo();
    std::array<vk::WriteDescriptorSet, 1> writeBuffer {
            vk::WriteDescriptorSet {
                    .dstSet = descriptorSet,
                    .dstBinding = bindingID,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = vk::DescriptorType::eUniformBuffer,
                    .pBufferInfo = &bufferInfo,
            }
    };
    {
        ZoneScopedN("driver.getLogicalDevice().updateDescriptorSets");
        driver.getLogicalDevice().updateDescriptorSets(writeBuffer, {});
    }
}

void Carrot::VulkanRenderer::bindBuffer(Pipeline& pipeline, const Render::Context& frame, const BufferView& view, std::uint32_t setID, std::uint32_t bindingID) {
    ZoneScoped;
    if(boundBuffers[{pipeline.getPipelineLayout(), frame.swapchainIndex, setID, bindingID}] == view) {
        return;
    }
    boundBuffers[{pipeline.getPipelineLayout(), frame.swapchainIndex, setID, bindingID}] = view;
    auto descriptorSet = pipeline.getDescriptorSets(frame, setID)[frame.swapchainIndex];

    auto bufferInfo = view.asBufferInfo();
    std::array<vk::WriteDescriptorSet, 1> writeBuffer {
            vk::WriteDescriptorSet {
                    .dstSet = descriptorSet,
                    .dstBinding = bindingID,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = vk::DescriptorType::eStorageBuffer,
                    .pBufferInfo = &bufferInfo,
            }
    };
    {
        ZoneScopedN("driver.getLogicalDevice().updateDescriptorSets");
        driver.getLogicalDevice().updateDescriptorSets(writeBuffer, {});
    }
}

void Carrot::VulkanRenderer::bindStorageImage(Carrot::Pipeline& pipeline, const Carrot::Render::Context& frame,
                                         const Carrot::Render::Texture& textureToBind,
                                         std::uint32_t setID,
                                         std::uint32_t bindingID,
                                         vk::ImageAspectFlags aspect,
                                         vk::ImageViewType viewType,
                                         std::uint32_t arrayIndex,
                                         vk::ImageLayout textureLayout) {
    ZoneScoped;
    if(boundStorageImages[{pipeline.getPipelineLayout(), frame.swapchainIndex, setID, bindingID, textureLayout}] == textureToBind.getVulkanImage()) {
        return;
    }
    boundStorageImages[{pipeline.getPipelineLayout(), frame.swapchainIndex, setID, bindingID, textureLayout}] = textureToBind.getVulkanImage();
    auto descriptorSet = pipeline.getDescriptorSets(frame, setID)[frame.swapchainIndex];

    vk::DescriptorImageInfo imageInfo {
            .imageView = textureToBind.getView(textureToBind.getImage().getFormat(), aspect, viewType),
            .imageLayout = textureLayout,
    };

    vk::DescriptorType descType = vk::DescriptorType::eStorageImage;

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

void Carrot::VulkanRenderer::bindTexture(Carrot::Pipeline& pipeline, const Carrot::Render::Context& frame,
                                         const Carrot::Render::Texture& textureToBind,
                                         std::uint32_t setID,
                                         std::uint32_t bindingID,
                                         vk::Sampler sampler,
                                         vk::ImageAspectFlags aspect,
                                         vk::ImageViewType viewType,
                                         std::uint32_t arrayIndex,
                                         vk::ImageLayout textureLayout) {
    // TODO: maybe interesting to batch these writes right before starting the rendering
    ZoneScoped;
    if(boundTextures[{pipeline.getPipelineLayout(), frame.swapchainIndex, setID, bindingID, textureLayout}] == textureToBind.getVulkanImage()) {
        return;
    }
    boundTextures[{pipeline.getPipelineLayout(), frame.swapchainIndex, setID, bindingID, textureLayout}] = textureToBind.getVulkanImage();
    auto descriptorSet = pipeline.getDescriptorSets(frame, setID)[frame.swapchainIndex];

    vk::DescriptorImageInfo imageInfo {
        .sampler = sampler,
        .imageView = textureToBind.getView(textureToBind.getImage().getFormat(), aspect, viewType),
        .imageLayout = textureLayout,
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
    ZoneScoped;

    frameCount++;

    {
        ZoneScopedN("ImGui_Impl new frame");
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
    }

    std::size_t previousCapacity = renderPackets.capacity();
    {
        ZoneScopedN("renderPackets.clear()");
        renderPackets.clear();
    }
    {
        ZoneScopedN("renderPackets.reserve(previousCapacity)");
        renderPackets.reserve(previousCapacity);
    }

    if(blinkTime > 0.0 && hasBlinked) {
        static auto lastTime = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<double> timeElapsed = now-lastTime;

        blinkTime -= timeElapsed.count();

        lastTime = now;
    }
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
    perDrawData.clear();
    perDrawOffsets.clear();
    {
        ZoneScopedN("Waiting on tasks required before starting the frame");
        mustBeDoneByNextFrameCounter.busyWait();
    }

    bool reloadedSomeShaders = false;
    for(auto& [name, pipePtrPtr] : pipelines.snapshot()) {
        reloadedSomeShaders |= (*pipePtrPtr)->checkForReloadableShaders();
    }

    // reloaded shaders -> pipeline recreation -> need to rebind descriptor
    if(reloadedSomeShaders) {
        boundBuffers.clear();
        boundTextures.clear();
        boundSamplers.clear();
        boundAS.clear();
    }

    Async::LockGuard lk { threadRegistrationLock };

    Async::Counter prepareThreadRenderPackets;
    for (auto& pair : perThreadPacketStorage.snapshot()) {
        TaskDescription task {
            .name = "Reset thread local render packet storage",
            .task = Async::AsTask<void>([pair]() {
                (*pair.second)->beginFrame();
            }),
            .joiner = &prepareThreadRenderPackets,
        };
        GetTaskScheduler().schedule(std::move(task), TaskScheduler::FrameParallelWork);
    }
    singleFrameAllocator.newFrame(renderContext.swapchainIndex);

    prepareThreadRenderPackets.busyWait();
    if(glfwGetKey(driver.getWindow().getGLFWPointer(), GLFW_KEY_F5) == GLFW_PRESS) {
        driver.breakOnNextVulkanError();
    }
}

struct PacketKey {
    Carrot::Pipeline* pipeline = nullptr;

    Carrot::Render::PassEnum pass = Carrot::Render::PassEnum::Undefined;
    Carrot::Render::Viewport* viewport = nullptr;

    Carrot::BufferView vertexBuffer;
    Carrot::BufferView indexBuffer;
    std::uint32_t indexCount = 0;
    // TODO: transparent GBuffer stuff

    bool operator==(const PacketKey& o) const = default;
};

inline void hash_combine(std::size_t& seed, const std::size_t& v) {
    seed ^= robin_hood::hash_int(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
}

template<>
struct std::hash<PacketKey> {
    std::size_t operator()(const PacketKey& key) const {
        const std::size_t p = 31;
#define hash_r(member) hash_combine(h, reinterpret_cast<std::size_t>((member)));
#define hash_s(member) hash_combine(h, static_cast<std::size_t>((member)));
        std::size_t h = 0;
        hash_r(key.pipeline);
        hash_s(key.pass);
        hash_r(key.viewport);
        hash_r((VkBuffer)key.vertexBuffer.getVulkanBuffer());
        hash_r((VkBuffer)key.vertexBuffer.getStart());
        hash_r((VkBuffer)key.vertexBuffer.getSize());
        hash_r((VkBuffer)key.indexBuffer.getVulkanBuffer());
        hash_r((VkBuffer)key.indexBuffer.getStart());
        hash_r((VkBuffer)key.indexBuffer.getSize());
        hash_s(key.indexCount);
        return h;
    }
};

PacketKey makeKey(const Carrot::Render::Packet& p) {
    return PacketKey {
        .pipeline = p.pipeline.get(),
        .pass = p.pass,
        .viewport = p.viewport,

        .vertexBuffer = p.vertexBuffer,
        .indexBuffer = p.indexBuffer,
//        .indexCount = p.indexCount,
    };
}

void Carrot::VulkanRenderer::beforeRecord(const Carrot::Render::Context& renderContext) {
    ZoneScoped;

    materialSystem.beginFrame(renderContext);
    lighting.beginFrame(renderContext);

    Async::LockGuard lk { threadRegistrationLock };

    bool open = true;
    bool debugRender = false;
    std::size_t totalPacketCount = 0;
    if(DebugRenderPacket) {
        debugRender = ImGui::Begin("Debug Render Packets", &open);
        if(!open) {
            DebugRenderPacket.setValue(false);
        }
    }

    static robin_hood::unordered_flat_map<PacketKey, std::vector<Carrot::Render::Packet>> packetBins;
    auto snapshot = threadRenderPackets.snapshot();

    auto placeInBin = [&](Carrot::Render::Packet&& toPlace) -> void {
        auto key = makeKey(toPlace);
        auto& bin = packetBins[key];

        for(auto& packet : bin) {
            // TODO: force merge(std::move(toPlace)) ?
            if(packet.merge(toPlace)) {
                return;
            }
        }

        // was not merged, brand new packet
        bin.emplace_back(std::move(toPlace));
    };

    for(const auto& [threadID, packets] : snapshot) {
        {
            ZoneScopedN("Move thread local packets to global bins");
            if(debugRender) {
                std::size_t packetCount = packets->unsorted.size();
                ImGui::Text("%llu packets from thread", packetCount);
                totalPacketCount += packetCount;
            }
            for(auto& p : packets->unsorted) {
                placeInBin(std::move(p));
            }
        }

        TaskDescription task {
                .name = "Cleanup thread local render packets",
                .task = Carrot::Async::AsTask<void>([pPackets = packets]() {
                    std::size_t previousCapacity = pPackets->unsorted.capacity();
                    {
                        ZoneScopedN("thread local packets.clear()");
                        pPackets->unsorted.clear();
                    }
                    {
                        ZoneScopedN("thread local packets.reserve(previousCapacity)");
                        pPackets->unsorted.reserve(previousCapacity);
                    }
                }),
                .joiner = &mustBeDoneByNextFrameCounter,
        };
        GetTaskScheduler().schedule(std::move(task), TaskScheduler::FrameParallelWork);
    }

    std::size_t previousCapacity = preparedRenderPackets.capacity();
    preparedRenderPackets.clear();
    preparedRenderPackets.reserve(previousCapacity);

    for(auto& [key, bin] : packetBins) {
        for(auto& packetOfBin : bin) {
            preparedRenderPackets.emplace_back(std::move(packetOfBin));
        }
        bin.clear();
    }

    sortRenderPackets(preparedRenderPackets);

    if(debugRender) {
        ImGui::Text("Render packet bin count: %llu", packetBins.size());
        ImGui::Text("Total render packets count: %llu", totalPacketCount);
        ImGui::Text("Merged render packets count: %llu", preparedRenderPackets.size());
        float ratio = static_cast<float>(preparedRenderPackets.size()) / static_cast<float>(totalPacketCount);
        ImGui::Separator();
        // TODO: add sorting time
//        ImGui::Text("Time for Render Packet merge: %0.3f ms", mergeTime*1000.0f);
        ImGui::Text("Draw call reduction: %0.1f%%", (1.0f - ratio)*100);
        ImGui::Text("Instance buffer size this frame: %s", Carrot::IO::getHumanReadableFileSize(singleFrameAllocator.getAllocatedSizeThisFrame()).c_str());
        ImGui::Text("Instance buffer size total: %s", Carrot::IO::getHumanReadableFileSize(singleFrameAllocator.getAllocatedSizeAllFrames()).c_str());
    }

    if(DebugRenderPacket) {
        ImGui::End();
    }

    hasBlinked = true;
}

void Carrot::VulkanRenderer::onFrame(const Carrot::Render::Context& renderContext) {
    ZoneScoped;

    {
        static DebugBufferObject obj{};
        auto& buffer = debugBuffers[renderContext.swapchainIndex];

        static std::uint32_t gBufferDebugType = DEBUG_GBUFFER_DISABLED;

        if(ShowGBuffer && &renderContext.viewport == &GetEngine().getMainViewport()) {
            bool open = true;
            if(ImGui::Begin("GBuffer debug", &open)) {
                static int gIndex = DEBUG_GBUFFER_DISABLED;
                ImGui::RadioButton("Disable debug", &gIndex, DEBUG_GBUFFER_DISABLED);
                ImGui::RadioButton("Albedo", &gIndex, DEBUG_GBUFFER_ALBEDO);
                ImGui::RadioButton("Positions", &gIndex, DEBUG_GBUFFER_POSITION);
                ImGui::RadioButton("Normals", &gIndex, DEBUG_GBUFFER_NORMAL);
                ImGui::RadioButton("Tangents", &gIndex, DEBUG_GBUFFER_TANGENT);
                ImGui::RadioButton("Depth", &gIndex, DEBUG_GBUFFER_DEPTH);
                ImGui::RadioButton("Metallic Roughness", &gIndex, DEBUG_GBUFFER_METALLIC_ROUGHNESS);
                ImGui::RadioButton("Emissive", &gIndex, DEBUG_GBUFFER_EMISSIVE);
                ImGui::RadioButton("Randomness", &gIndex, DEBUG_GBUFFER_RANDOMNESS);
                ImGui::RadioButton("Motion", &gIndex, DEBUG_GBUFFER_MOTION);
                ImGui::RadioButton("Moments (RG), Reprojection history length (B)", &gIndex, DEBUG_GBUFFER_MOMENTS);
                ImGui::RadioButton("EntityID", &gIndex, DEBUG_GBUFFER_ENTITYID);
                ImGui::RadioButton("Lighting", &gIndex, DEBUG_GBUFFER_LIGHTING);
                ImGui::RadioButton("Noisy lighting", &gIndex, DEBUG_GBUFFER_NOISY_LIGHTING);
                ImGui::RadioButton("Temporal denoise result", &gIndex, DEBUG_POST_TEMPORAL_DENOISE);
                ImGui::RadioButton("Temporal denoise result (with firefly rejection)", &gIndex, DEBUG_POST_FIREFLY_REJECTION);
                ImGui::RadioButton("Variance", &gIndex, DEBUG_VARIANCE);

                obj.gBufferType = gIndex;
            }
            ImGui::End();

            if(!open) {
                ShowGBuffer.setValue(false);
            }
        }

        buffer->directUpload(&obj, sizeof(obj));
    }

    asBuilder->onFrame(renderContext);
}

Carrot::Engine& Carrot::VulkanRenderer::getEngine() {
    return driver.getEngine();
}

void Carrot::VulkanRenderer::createCameraSetResources() {
    vk::DescriptorSetLayoutBinding cameraBindings[] = {
            {
                    .binding = 0,
                    .descriptorType = vk::DescriptorType::eUniformBuffer,
                    .descriptorCount = 1,
                    .stageFlags = vk::ShaderStageFlagBits::eAllGraphics | vk::ShaderStageFlagBits::eRaygenKHR,
            },
            {
                    .binding = 1,
                    .descriptorType = vk::DescriptorType::eUniformBuffer,
                    .descriptorCount = 1,
                    .stageFlags = vk::ShaderStageFlagBits::eAllGraphics | vk::ShaderStageFlagBits::eRaygenKHR,
            }
    };
    cameraDescriptorSetLayout = getVulkanDriver().getLogicalDevice().createDescriptorSetLayoutUnique(vk::DescriptorSetLayoutCreateInfo {
            .bindingCount = 2,
            .pBindings = cameraBindings,
    });

    vk::DescriptorPoolSize poolSize {
            .type = vk::DescriptorType::eUniformBuffer,
            .descriptorCount = 2,
    };
    cameraDescriptorPool = getVulkanDriver().getLogicalDevice().createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo {
            .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet | vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind,
            .maxSets = static_cast<uint32_t>(getSwapchainImageCount()) * MaxCameras,
            .poolSizeCount = 1,
            .pPoolSizes = &poolSize,
    });
}

void Carrot::VulkanRenderer::createViewportSetResources() {
    vk::DescriptorSetLayoutBinding bindings[] = {
            {
                    .binding = 0,
                    .descriptorType = vk::DescriptorType::eUniformBuffer,
                    .descriptorCount = 1,
                    .stageFlags = vk::ShaderStageFlagBits::eAllGraphics,
            }
    };
    viewportDescriptorSetLayout = getVulkanDriver().getLogicalDevice().createDescriptorSetLayoutUnique(vk::DescriptorSetLayoutCreateInfo {
            .bindingCount = 1,
            .pBindings = bindings,
    });

    vk::DescriptorPoolSize poolSize {
            .type = vk::DescriptorType::eUniformBuffer,
            .descriptorCount = 1,
    };
    viewportDescriptorPool = getVulkanDriver().getLogicalDevice().createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo {
            .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet | vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind,
            .maxSets = static_cast<uint32_t>(getSwapchainImageCount()) * MaxViewports,
            .poolSizeCount = 1,
            .pPoolSizes = &poolSize,
    });
}

void Carrot::VulkanRenderer::createPerDrawSetResources() {
    vk::DescriptorSetLayoutBinding bindings[] = {
            {
                    .binding = 0,
                    .descriptorType = vk::DescriptorType::eStorageBuffer,
                    .descriptorCount = 1,
                    .stageFlags = vk::ShaderStageFlagBits::eAllGraphics,
            },
            {
                    .binding = 1,
                    .descriptorType = vk::DescriptorType::eUniformBufferDynamic,
                    .descriptorCount = 1,
                    .stageFlags = vk::ShaderStageFlagBits::eAllGraphics,
            }
    };
    perDrawDescriptorSetLayout = getVulkanDriver().getLogicalDevice().createDescriptorSetLayoutUnique(vk::DescriptorSetLayoutCreateInfo {
            .bindingCount = 2,
            .pBindings = bindings,
    });

    vk::DescriptorPoolSize poolSizes[2] {
            {
                    .type = vk::DescriptorType::eStorageBuffer,
                    .descriptorCount = 1,
            },
            {
                    .type = vk::DescriptorType::eUniformBufferDynamic,
                    .descriptorCount = 1,
            }
    };
    perDrawDescriptorPool = getVulkanDriver().getLogicalDevice().createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo {
            .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet | vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind,
            .maxSets = static_cast<uint32_t>(getSwapchainImageCount()),
            .poolSizeCount = 2,
            .pPoolSizes = poolSizes,
    });

    // allocate default buffers

    std::size_t count = getSwapchainImageCount();
    std::vector<vk::DescriptorSetLayout> layouts {count, *perDrawDescriptorSetLayout};
    perDrawDescriptorSets = getVulkanDriver().getLogicalDevice().allocateDescriptorSets(vk::DescriptorSetAllocateInfo {
            .descriptorPool = *perDrawDescriptorPool,
            .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
            .pSetLayouts = layouts.data(),
    });

    perDrawBuffers.resize(getSwapchainImageCount());
    perDrawOffsetBuffers.resize(getSwapchainImageCount());

    for(auto& set : perDrawDescriptorSets) {
        std::vector<vk::WriteDescriptorSet> writes = {
                {
                        .dstSet = set,
                        .dstBinding = 0,
                        .descriptorCount = 1,
                        .descriptorType = vk::DescriptorType::eStorageBuffer,
                        .pBufferInfo = &nullBufferInfo,
                },
                {
                        .dstSet = set,
                        .dstBinding = 1,
                        .descriptorCount = 1,
                        .descriptorType = vk::DescriptorType::eUniformBufferDynamic,
                        .pBufferInfo = &nullBufferInfo,
                }
        };

        getVulkanDriver().getLogicalDevice().updateDescriptorSets(writes, {});
    }
}

void Carrot::VulkanRenderer::updatePerDrawBuffers(const Carrot::Render::Context& renderContext) {
    ZoneScoped;
    const std::size_t elementSize = sizeof(GBufferDrawData);
    const std::size_t requiredStorage = perDrawData.size() * elementSize;
    if(requiredStorage == 0)
        return;

    const std::size_t frameIndex = renderContext.swapchainIndex;
    if(perDrawBuffers[frameIndex] == nullptr || perDrawBuffers[frameIndex]->getSize() < requiredStorage) {
        perDrawBuffers[frameIndex] = getEngine().getResourceAllocator().allocateDedicatedBuffer(requiredStorage,
                                                                                                vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
                                                                                                vk::MemoryPropertyFlagBits::eDeviceLocal);

        perDrawBuffers[frameIndex]->setDebugNames(Carrot::sprintf("Per-draw buffer frame %llu", frameIndex));

        vk::DescriptorBufferInfo buffer = perDrawBuffers[frameIndex]->asBufferInfo();
        auto write = vk::WriteDescriptorSet {
            .dstSet = perDrawDescriptorSets[frameIndex],
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eStorageBuffer,
            .pBufferInfo = &buffer,
        };

        getVulkanDriver().getLogicalDevice().updateDescriptorSets(write, {});
    }

    const std::size_t alignedElementSize = Carrot::Math::alignUp(sizeof(std::uint32_t), GetVulkanDriver().getPhysicalDeviceLimits().minUniformBufferOffsetAlignment);
    const std::size_t offsetRequiredStorage = perDrawOffsets.size() * alignedElementSize;

    if(perDrawOffsetBuffers[frameIndex] == nullptr || perDrawOffsetBuffers[frameIndex]->getSize() < offsetRequiredStorage) {
        perDrawOffsetBuffers[frameIndex] = getEngine().getResourceAllocator().allocateDedicatedBuffer(offsetRequiredStorage,
                                                                                                vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst,
                                                                                                vk::MemoryPropertyFlagBits::eDeviceLocal);
        perDrawOffsetBuffers[frameIndex]->setDebugNames(Carrot::sprintf("Per-draw offsets frame %llu", frameIndex));
        vk::DescriptorBufferInfo buffer = perDrawOffsetBuffers[frameIndex]->asBufferInfo();
        buffer.range = alignedElementSize;
        auto write = vk::WriteDescriptorSet {
            .dstSet = perDrawDescriptorSets[frameIndex],
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eUniformBufferDynamic,
            .pBufferInfo = &buffer,
        };

        getVulkanDriver().getLogicalDevice().updateDescriptorSets(write, {});
    }

    std::size_t index = 0;
    std::vector<std::pair<std::size_t, std::span<const std::uint32_t>>> copyTargets;
    copyTargets.reserve(perDrawOffsets.size());
    for(const auto& element : perDrawOffsets) {
        const std::size_t offset = index * alignedElementSize;

        copyTargets.emplace_back(offset, std::span{&element, 1});
        index++;
    }

    // TODO: async upload
    perDrawOffsetBuffers[frameIndex]->stageUploadWithOffsets(std::span { copyTargets });
    perDrawBuffers[frameIndex]->stageUploadWithOffset(0ull, perDrawData.data(), perDrawData.size() * sizeof(GBufferDrawData));
}

/*
void Carrot::VulkanRenderer::updateIndirectDrawCommands(const Carrot::Render::Context& renderContext) {
    TODO;
}*/

void Carrot::VulkanRenderer::createDebugSetResources() {
    vk::DescriptorSetLayoutBinding debugBinding {
            .binding = 0,
            .descriptorType = vk::DescriptorType::eUniformBuffer,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eAllGraphics | vk::ShaderStageFlagBits::eRaygenKHR,
    };
    debugDescriptorSetLayout = getVulkanDriver().getLogicalDevice().createDescriptorSetLayoutUnique(vk::DescriptorSetLayoutCreateInfo {
            .bindingCount = 1,
            .pBindings = &debugBinding,
    });

    vk::DescriptorPoolSize poolSize {
            .type = vk::DescriptorType::eUniformBuffer,
            .descriptorCount = 1,
    };
    debugDescriptorPool = getVulkanDriver().getLogicalDevice().createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo {
            .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            .maxSets = static_cast<uint32_t>(getSwapchainImageCount()) * MaxCameras,
            .poolSizeCount = 1,
            .pPoolSizes = &poolSize,
    });

    std::size_t count = GetEngine().getSwapchainImageCount();
    std::vector<vk::DescriptorSetLayout> layouts {count, *debugDescriptorSetLayout};
    debugDescriptorSets = getVulkanDriver().getLogicalDevice().allocateDescriptorSets(vk::DescriptorSetAllocateInfo {
            .descriptorPool = *debugDescriptorPool,
            .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
            .pSetLayouts = layouts.data(),
    });

    debugBuffers.resize(debugDescriptorSets.size());
    for (int i = 0; i < debugDescriptorSets.size(); i++) {
        debugBuffers[i] = GetResourceAllocator().allocateDedicatedBuffer(
                sizeof(DebugBufferObject),
                vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eUniformBuffer,
                vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible
                );

        vk::DescriptorBufferInfo cameraBuffer = debugBuffers[i]->asBufferInfo();
        vk::WriteDescriptorSet write {
                .dstSet = debugDescriptorSets[i],
                .dstBinding = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eUniformBuffer,
                .pBufferInfo = &cameraBuffer,
        };

        getVulkanDriver().getLogicalDevice().updateDescriptorSets(write, {});
    }
}

void Carrot::VulkanRenderer::createDefaultResources() {
    std::unique_ptr<Image> cubeMap = std::make_unique<Image>(
            GetVulkanDriver(),
            vk::Extent3D{1,1,1},
            vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
            vk::Format::eR8G8B8A8Unorm,
            GetVulkanDriver().createGraphicsAndTransferFamiliesSet(),
            vk::ImageCreateFlagBits::eCubeCompatible,
            vk::ImageType::e2D,
            6);

    std::array<std::uint8_t, 4> blackPixel = {0,0,0,0};
    cubeMap->stageUpload(blackPixel, 0);
    cubeMap->stageUpload(blackPixel, 1);
    cubeMap->stageUpload(blackPixel, 2);
    cubeMap->stageUpload(blackPixel, 3);
    cubeMap->stageUpload(blackPixel, 4);
    cubeMap->stageUpload(blackPixel, 5);

    blackCubeMapTexture = std::make_shared<Render::Texture>(std::move(cubeMap));

    forwardRenderingFrameInfo.clear();
    forwardRenderingFrameInfo.resize(getSwapchainImageCount());
    for (std::size_t j = 0; j < getSwapchainImageCount(); ++j) {
        forwardRenderingFrameInfo[j] = getEngine().getResourceAllocator().allocateDedicatedBuffer(
                sizeof(ForwardFrameInfo),
                vk::BufferUsageFlagBits::eUniformBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
                );
    }
}

std::vector<vk::DescriptorSet> Carrot::VulkanRenderer::createDescriptorSetForCamera(const std::vector<Carrot::BufferView>& uniformBuffers) {
    verify(uniformBuffers.size() > 0, "Must have at least one uniform buffer");
    int vrMultiplier = (GetEngine().getConfiguration().runInVR ? 2 : 1);
    std::size_t count = getSwapchainImageCount() * vrMultiplier;
    std::vector<vk::DescriptorSetLayout> layouts {count, *cameraDescriptorSetLayout};
    auto cameraDescriptorSets = getVulkanDriver().getLogicalDevice().allocateDescriptorSets(vk::DescriptorSetAllocateInfo {
            .descriptorPool = *cameraDescriptorPool,
            .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
            .pSetLayouts = layouts.data(),
    });

    verify(cameraDescriptorSets.size() == uniformBuffers.size(), "mismatched camera descriptor count and uniform buffer count");
    for (int i = 0; i < cameraDescriptorSets.size(); i++) {
        int nextFrameIndex = (i + 2 * vrMultiplier) % (getSwapchainImageCount() * vrMultiplier);
        vk::DescriptorBufferInfo cameraBuffer = uniformBuffers[i].asBufferInfo();
        vk::DescriptorBufferInfo previousCameraBuffer = uniformBuffers[nextFrameIndex].asBufferInfo();
        std::vector<vk::WriteDescriptorSet> writes = {
                {
                        .dstSet = cameraDescriptorSets[i],
                        .dstBinding = 0,
                        .descriptorCount = 1,
                        .descriptorType = vk::DescriptorType::eUniformBuffer,
                        .pBufferInfo = &cameraBuffer,
                },
                {
                        .dstSet = cameraDescriptorSets[i],
                        .dstBinding = 1,
                        .descriptorCount = 1,
                        .descriptorType = vk::DescriptorType::eUniformBuffer,
                        .pBufferInfo = &previousCameraBuffer,
                }
        };

        getVulkanDriver().getLogicalDevice().updateDescriptorSets(writes, {});
    }
    return cameraDescriptorSets;
}

void Carrot::VulkanRenderer::destroyCameraDescriptorSets(const std::vector<vk::DescriptorSet>& sets) {
    if(sets.empty())
        return;
    getVulkanDriver().getLogicalDevice().freeDescriptorSets(*cameraDescriptorPool, sets);
}

std::vector<vk::DescriptorSet> Carrot::VulkanRenderer::createDescriptorSetForViewport(const std::vector<Carrot::BufferView>& uniformBuffers) {
    verify(uniformBuffers.size() > 0, "Must have at least one uniform buffer");
    std::size_t count = getSwapchainImageCount();
    std::vector<vk::DescriptorSetLayout> layouts {count, *viewportDescriptorSetLayout};
    auto descriptorSets = getVulkanDriver().getLogicalDevice().allocateDescriptorSets(vk::DescriptorSetAllocateInfo {
            .descriptorPool = *viewportDescriptorPool,
            .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
            .pSetLayouts = layouts.data(),
    });

    verify(descriptorSets.size() == uniformBuffers.size(), "mismatched camera descriptor count and uniform buffer count");
    for (int i = 0; i < descriptorSets.size(); i++) {
        vk::DescriptorBufferInfo buffer = uniformBuffers[i].asBufferInfo();
        std::vector<vk::WriteDescriptorSet> writes = {
                {
                        .dstSet = descriptorSets[i],
                        .dstBinding = 0,
                        .descriptorCount = 1,
                        .descriptorType = vk::DescriptorType::eUniformBuffer,
                        .pBufferInfo = &buffer,
                }
        };

        getVulkanDriver().getLogicalDevice().updateDescriptorSets(writes, {});
    }
    return descriptorSets;
}

void Carrot::VulkanRenderer::destroyViewportDescriptorSets(const std::vector<vk::DescriptorSet>& sets) {
    if(sets.empty())
        return;
    getVulkanDriver().getLogicalDevice().freeDescriptorSets(*viewportDescriptorPool, sets);
}

const vk::DescriptorSetLayout& Carrot::VulkanRenderer::getCameraDescriptorSetLayout() const {
    return *cameraDescriptorSetLayout;
}

const vk::DescriptorSetLayout& Carrot::VulkanRenderer::getDebugDescriptorSetLayout() const {
    return *debugDescriptorSetLayout;
}

const vk::DescriptorSetLayout& Carrot::VulkanRenderer::getViewportDescriptorSetLayout() const {
    return *viewportDescriptorSetLayout;
}

const vk::DescriptorSetLayout& Carrot::VulkanRenderer::getPerDrawDescriptorSetLayout() const {
    return *perDrawDescriptorSetLayout;
}

const vk::DescriptorSet& Carrot::VulkanRenderer::getDebugDescriptorSet(const Render::Context& renderContext) const {
    return debugDescriptorSets[renderContext.swapchainIndex];
}

const vk::DescriptorSet& Carrot::VulkanRenderer::getPerDrawDescriptorSet(const Render::Context& renderContext) const {
    return perDrawDescriptorSets[renderContext.swapchainIndex];
}

void Carrot::VulkanRenderer::fullscreenBlit(const vk::RenderPass& pass, const Carrot::Render::Context& frame, Carrot::Render::Texture& textureToBlit, Carrot::Render::Texture& targetTexture, vk::CommandBuffer& cmds) {
    auto pipeline = getOrCreateRenderPassSpecificPipeline("blit", pass);
    vk::DescriptorImageInfo imageInfo {
            .sampler = getVulkanDriver().getLinearSampler(),
            .imageView = textureToBlit.getView(),
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
    };

    auto set = pipeline->getDescriptorSets(frame, 0)[frame.swapchainIndex];
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

Carrot::Render::Texture::Ref Carrot::VulkanRenderer::getBlackCubeMapTexture() {
    return blackCubeMapTexture;
}

void Carrot::VulkanRenderer::recordOpaqueGBufferPass(vk::RenderPass pass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) {
    ZoneScoped;

    if(blinkTime > 0.0) {
        return;
    }

    Carrot::Render::Viewport* viewport = &renderContext.viewport;
    verify(viewport, "Viewport cannot be null");

    auto packets = getRenderPackets(viewport, Carrot::Render::PassEnum::OpaqueGBuffer);
    const Carrot::Render::Packet* previousPacket = nullptr;
    for(const auto& p : packets) {
        p.record(pass, renderContext, commands, previousPacket);
        previousPacket = &p;
    }
}

void Carrot::VulkanRenderer::recordTransparentGBufferPass(vk::RenderPass pass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) {
    ZoneScoped;

    if(blinkTime > 0.0) {
        return;
    }

    const bool useRaytracingVersion = GetEngine().getCapabilities().supportsRaytracing;
    std::shared_ptr<Carrot::Pipeline> forwardPipeline;
    if(useRaytracingVersion) {
        forwardPipeline = getOrCreatePipeline("forward-raytracing");
    } else {
        forwardPipeline = getOrCreatePipeline("forward-noraytracing");
    }

    Render::Texture::Ref skyboxCubeMap = GetEngine().getSkyboxCubeMap();
    if(!skyboxCubeMap || GetEngine().getSkybox() == Carrot::Skybox::Type::None) {
        skyboxCubeMap = getBlackCubeMapTexture();
    }
    bindTexture(*forwardPipeline, renderContext, *skyboxCubeMap, 5, 0, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::eCube);

    auto& forwardRenderingFrameInfoBuffer = forwardRenderingFrameInfo[renderContext.swapchainIndex];

    ForwardFrameInfo frameInfo {
            .frameCount = getFrameCount(),
            .frameWidth = GetVulkanDriver().getWindowFramebufferExtent().width,
            .frameHeight = GetVulkanDriver().getWindowFramebufferExtent().height,

            .hasTLAS = getASBuilder().getTopLevelAS() != nullptr
    };

    forwardRenderingFrameInfoBuffer->directUpload(&frameInfo, sizeof(frameInfo));
    bindUniformBuffer(*forwardPipeline, renderContext, forwardRenderingFrameInfoBuffer->getWholeView(), 5, 1);
    if(useRaytracingVersion) {
        auto& tlas = getASBuilder().getTopLevelAS();
        if(tlas) {
            bindAccelerationStructure(*forwardPipeline, renderContext, *tlas, 5, 2);
            bindTexture(*forwardPipeline, renderContext, *getMaterialSystem().getBlueNoiseTextures()[renderContext.swapchainIndex % Render::BlueNoiseTextureCount]->texture, 5, 3, nullptr);
            bindBuffer(*forwardPipeline, renderContext, getASBuilder().getGeometriesBuffer(renderContext), 5, 4);
            bindBuffer(*forwardPipeline, renderContext, getASBuilder().getInstancesBuffer(renderContext), 5, 5);
        }
    }

    Carrot::Render::Viewport* viewport = &renderContext.viewport;
    verify(viewport, "Viewport cannot be null");

    auto packets = getRenderPackets(viewport, Carrot::Render::PassEnum::TransparentGBuffer);
    const Carrot::Render::Packet* previousPacket = nullptr;
    for(const auto& p : packets) {
        p.record(pass, renderContext, commands, previousPacket);
        previousPacket = &p;
    }
}

std::span<const Carrot::Render::Packet> Carrot::VulkanRenderer::getRenderPackets(Carrot::Render::Viewport* viewport, Carrot::Render::PassEnum pass) const {
    ZoneScoped;
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

void Carrot::VulkanRenderer::collectRenderPackets() {
    ZoneScoped;
    Async::LockGuard lk { threadRegistrationLock };

    auto snapshot = threadRenderPackets.snapshot();

    for(const auto& [threadID, packets] : snapshot) {
        {
            ZoneScopedN("Move thread local packets to global vector");
            for(auto& p : packets->unsorted) {
                renderPackets.emplace_back(std::move(p));
            }
        }

        std::size_t previousCapacity = packets->unsorted.capacity();
        {
            ZoneScopedN("thread local packets.clear()");
            packets->unsorted.clear();
        }
        {
            ZoneScopedN("thread local packets.reserve(previousCapacity)");
            packets->unsorted.reserve(previousCapacity);
        }
    }
}

void Carrot::VulkanRenderer::sortRenderPackets(std::vector<Carrot::Render::Packet>& inputPackets) {
    ZoneScoped;
    // sort by viewport, pass, then pipeline, then mesh
    std::sort(std::execution::par_unseq, inputPackets.begin(), inputPackets.end(), [&](const Render::Packet& a, const Render::Packet& b) {
        if(a.viewport != b.viewport) {
            return a.viewport < b.viewport;
        }

        if(a.pass != b.pass) {
            return a.pass < b.pass;
        }

        if(a.transparentGBuffer.zOrder != b.transparentGBuffer.zOrder) {
            return a.transparentGBuffer.zOrder < b.transparentGBuffer.zOrder;
        }

        if(a.pipeline != b.pipeline) {
            return a.pipeline < b.pipeline;
        }

        return a.vertexBuffer.getVulkanBuffer() < b.vertexBuffer.getVulkanBuffer();
    });
}

void Carrot::VulkanRenderer::mergeRenderPackets(const std::vector<Carrot::Render::Packet>& inputPackets, std::vector<Carrot::Render::Packet>& outputPackets) {
    ZoneScoped;
    std::size_t previousCapacity = outputPackets.capacity();
    outputPackets.clear();
    outputPackets.reserve(previousCapacity);

    if(inputPackets.empty()) {
        return;
    }

    static bool doMerge = true;
    float mergeTime = 0.0f;

    {
        Carrot::Profiling::ScopedTimer mergeTimer("Merge timer");
        Render::Packet mergeResult = inputPackets[0];
        for(std::size_t i = 1; i < inputPackets.size(); i++) {
            const Render::Packet& currentPacket = inputPackets[i];
            if(!doMerge || !mergeResult.merge(currentPacket)) {
                outputPackets.push_back(mergeResult);
                mergeResult = currentPacket;
            }
        }

        outputPackets.push_back(mergeResult);

        mergeTime = mergeTimer.getTime();
    }

    if(DebugRenderPacket) {
        bool open = true;
        if(ImGui::Begin("Debug Render Packets", &open)) {
            // TODO: separate per thread
            if(!open) {
                DebugRenderPacket.setValue(false);
            }

            ImGui::Checkbox("Merge Render Packets with instancing", &doMerge);
            ImGui::Text("Total render packets count: %llu", inputPackets.size());
            ImGui::Text("Merged render packets count: %llu", outputPackets.size());
            float ratio = static_cast<float>(outputPackets.size()) / static_cast<float>(inputPackets.size());
            ImGui::Separator();
            // TODO: add sorting time
            ImGui::Text("Time for Render Packet merge: %0.3f ms", mergeTime*1000.0f);
            ImGui::Text("Draw call reduction: %0.1f%%", (1.0f - ratio)*100);
            ImGui::Text("Instance buffer size this frame: %s", Carrot::IO::getHumanReadableFileSize(singleFrameAllocator.getAllocatedSizeThisFrame()).c_str());
            ImGui::Text("Instance buffer size total: %s", Carrot::IO::getHumanReadableFileSize(singleFrameAllocator.getAllocatedSizeAllFrames()).c_str());
        }
        ImGui::End();
    }
}

void Carrot::VulkanRenderer::renderWireframeSphere(const Carrot::Render::Context& renderContext, const glm::mat4& transform, float radius, const glm::vec4& color, const Carrot::UUID& objectID) {
    renderWireframe(*unitSphereModel, renderContext, transform, color, objectID);
}

void Carrot::VulkanRenderer::renderWireframeCapsule(const Carrot::Render::Context& renderContext, const glm::mat4& transform, float radius, float height, const glm::vec4& color, const Carrot::UUID& objectID) {
    renderWireframe(*unitCapsuleModel, renderContext, transform, color, objectID);
}

void Carrot::VulkanRenderer::renderWireframeCuboid(const Carrot::Render::Context& renderContext, const glm::mat4& transform, const glm::vec3& halfExtents, const glm::vec4& color, const Carrot::UUID& objectID) {
    renderWireframe(*unitCubeModel, renderContext, transform, color, objectID);
}

void Carrot::VulkanRenderer::renderWireframe(const Carrot::Model& model, const Carrot::Render::Context& renderContext, const glm::mat4& transform, const glm::vec4& color, const Carrot::UUID& objectID) {
    Render::Packet& packet = GetRenderer().makeRenderPacket(Carrot::Render::PassEnum::OpaqueGBuffer, renderContext.viewport);
    Carrot::GBufferDrawData data;
    data.materialIndex = whiteMaterial->getSlot();

    packet.useMesh(*model.getStaticMeshes()[0]); // TODO: find a better way to load individual meshes
    packet.pipeline = wireframeGBufferPipeline;

    Carrot::Render::Packet::PushConstant& pushConstant = packet.addPushConstant();
    pushConstant.id = "drawDataPush";
    pushConstant.stages = vk::ShaderStageFlagBits::eFragment;

    pushConstant.setData(data);

    Carrot::InstanceData instance;
    instance.uuid = objectID;
    instance.color = color;
    instance.transform = transform;
    packet.useInstance(instance);
    render(packet);
}

std::uint32_t Carrot::VulkanRenderer::uploadPerDrawData(const std::span<GBufferDrawData>& drawData) {
    const std::size_t alignedElementSize = Math::alignUp(sizeof(std::uint32_t), driver.getPhysicalDeviceLimits().minUniformBufferOffsetAlignment);

    std::size_t offset = perDrawOffsets.size() * alignedElementSize;
    perDrawOffsets.emplace_back(static_cast<std::uint32_t>(perDrawData.size()));

    for(const auto& data : drawData) {
        perDrawData.emplace_back(data);
    }
    return offset;
}

std::shared_ptr<Carrot::Model> Carrot::VulkanRenderer::getUnitSphere() {
    return unitSphereModel;
}

std::shared_ptr<Carrot::Model> Carrot::VulkanRenderer::getUnitCapsule() {
    return unitCapsuleModel;
}

std::shared_ptr<Carrot::Model> Carrot::VulkanRenderer::getUnitCube() {
    return unitCubeModel;
}

Carrot::Render::Packet& Carrot::VulkanRenderer::makeRenderPacket(Render::PassEnum pass, Render::Viewport& viewport, std::source_location location) {
    return threadLocalPacketStorage->make(pass, &viewport, location);
}

void Carrot::VulkanRenderer::render(const Render::Packet& packet) {
    ZoneScopedN("Queue RenderPacket");
    packet.validate();
    verify(threadLocalRenderPackets != nullptr, "Current thread must have been registered via VulkanRenderer::makeCurrentThreadRenderCapable()");
    threadLocalRenderPackets->unsorted.emplace_back(packet);
}

void Carrot::VulkanRenderer::blink() {
    blinkTime = BlinkDuration;
    hasBlinked = false;
}

Carrot::BufferView Carrot::VulkanRenderer::getSingleFrameBuffer(vk::DeviceSize bytes) {
    return singleFrameAllocator.allocate(bytes);
}

Carrot::BufferView Carrot::VulkanRenderer::getInstanceBuffer(vk::DeviceSize bytes) {
    // TODO: use different allocator? (even if only for tracking)
    return singleFrameAllocator.allocate(bytes);
}

Carrot::Async::Task<std::shared_ptr<Carrot::Model>> Carrot::VulkanRenderer::coloadModel(
        /* not a ref because we need the string to be alive inside the coroutine*/ std::string name) {
    try {
        auto result = getOrCreateModel(name);
        co_return result;
    } catch (...) {
        throw;
    }
}

void Carrot::VulkanRenderer::makeCurrentThreadRenderCapable() {
    Async::LockGuard lk { threadRegistrationLock };
    auto& packetsEntry = threadRenderPackets.getOrCompute(std::this_thread::get_id(), [](){ return ThreadPackets{}; });
    auto& storageEntry = perThreadPacketStorage.getOrCompute(std::this_thread::get_id(), [](){ return std::make_unique<Render::PacketContainer>(); });
    threadLocalRenderPackets = &packetsEntry;
    threadLocalPacketStorage = storageEntry.get();
}