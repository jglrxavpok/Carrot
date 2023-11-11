//
// Created by jglrxavpok on 13/03/2021.
//

#include "VulkanRenderer.h"
#include "GBuffer.h"
#include "engine/render/raytracing/ASBuilder.h"
#include "engine/render/raytracing/RayTracer.h"
#include "engine/console/RuntimeOption.hpp"
#include "core/utils/Assert.h"
#include "core/async/OSThreads.h"
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
#include <IconsFontAwesome5.h>
#include <engine/console/Console.h>

static constexpr std::size_t SingleFrameAllocatorSize = 512 * 1024 * 1024; // 512Mb per frame-in-flight
static Carrot::RuntimeOption DebugRenderPacket("Debug Render Packets", false);
static Carrot::RuntimeOption ShowGBuffer("Engine/Show GBuffer", false);

static thread_local Carrot::VulkanRenderer::ThreadPackets* threadLocalRenderPackets = nullptr;
static thread_local std::array<Carrot::Render::PacketContainer, 2>* threadLocalPacketStorage = nullptr;

static std::thread::id RenderThreadID{};

#if 0
#define ASSERT_NOT_RENDER_THREAD() verify(std::this_thread::get_id() != RenderThreadID, "Only valid on non-Render threads!")
#define ASSERT_RENDER_THREAD() verify(std::this_thread::get_id() == RenderThreadID, "Only valid on Render threads!")
#else
#define ASSERT_NOT_RENDER_THREAD()
#define ASSERT_RENDER_THREAD()
#endif

struct ForwardFrameInfo {
    std::uint32_t frameCount;
    std::uint32_t frameWidth;
    std::uint32_t frameHeight;

    bool hasTLAS;
};

Carrot::VulkanRenderer::VulkanRenderer(VulkanDriver& driver, Configuration config): driver(driver), config(config),
                                                                                    recordingRenderContext(*this, nullptr),
                                                                                    singleFrameAllocator(SingleFrameAllocatorSize),
                                                                                    imGuiBackend(*this)
{
    ZoneScoped;
    renderThreadKickoff.increment();
    renderThreadReady.increment();
    renderThread = std::jthread([&]() {
        renderThreadProc();
    });
    Carrot::Threads::setName(renderThread, "Carrot Render Thread");

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

Carrot::VulkanRenderer::~VulkanRenderer() {
    waitForRenderToComplete(); // wait for last render job
    running = false; // ask render thread to stop
    renderThreadKickoff.decrement(); // tell render thread to wake up
    renderThread.join(); // wait for render thread to stop

    GetAssetServer().freeupResources();
}

void Carrot::VulkanRenderer::lateInit() {
    ZoneScoped;
    unitSphereModel = GetAssetServer().blockingLoadModel("resources/models/simple_sphere.obj");
    unitCubeModel = GetAssetServer().blockingLoadModel("resources/models/simple_cube.obj");
    unitCapsuleModel = GetAssetServer().blockingLoadModel("resources/models/simple_capsule.obj");
    wireframeGBufferPipeline = getOrCreatePipeline("gBufferWireframe");
    gBufferPipeline = getOrCreatePipeline("gBuffer");
    whiteMaterial = getMaterialSystem().createMaterialHandle();
    whiteMaterial->albedo = getMaterialSystem().getWhiteTexture();

    // requires whiteMaterial
    debugArrowModel = GetAssetServer().blockingLoadModel("resources/models/simple_arrow.gltf");
}

std::shared_ptr<Carrot::Pipeline> Carrot::VulkanRenderer::getOrCreateRenderPassSpecificPipeline(const std::string& name, const vk::RenderPass& renderPass) {
    return getOrCreatePipeline(name, reinterpret_cast<std::uint64_t>((void*) &renderPass));
}

std::shared_ptr<Carrot::Pipeline> Carrot::VulkanRenderer::getOrCreatePipeline(const std::string& name, std::uint64_t instanceOffset) {
    return getOrCreatePipelineFullPath("resources/pipelines/"+name+".json", instanceOffset);
}

std::shared_ptr<Carrot::Pipeline> Carrot::VulkanRenderer::getOrCreatePipelineFullPath(const std::string& name, std::uint64_t instanceOffset) {
    return GetAssetServer().blockingLoadPipeline(Carrot::IO::VFS::Path { name }, instanceOffset);
}

std::shared_ptr<Carrot::Render::Texture> Carrot::VulkanRenderer::getOrCreateTexture(const std::string& textureName) {
    return GetAssetServer().blockingLoadTexture(Carrot::IO::VFS::Path { "resources/textures/" + textureName });
}

std::shared_ptr<Carrot::Render::Font> Carrot::VulkanRenderer::getOrCreateFront(const Carrot::IO::Resource& from) {
    if(from.isFile()) {
        const auto& fontName = from.getName();
        ZoneScopedN("Loading font");
        ZoneText(fontName.c_str(), fontName.size());
        return GetAssetServer().fonts.getOrCompute(fontName, [&]() {
            return std::make_shared<Carrot::Render::Font>(*this, from);
        });
    } else {
        auto font = std::make_shared<Carrot::Render::Font>(*this, from);
        return font;
    }
}

void Carrot::VulkanRenderer::recreateDescriptorPools(size_t frameCount) {
    for(const auto& [nameHash, pipelinePtrPtr] : GetAssetServer().pipelines.snapshot()) {
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

    {
        auto& style = ImGui::GetStyle();
        style.FrameRounding = 2.0f;
        style.FrameBorderSize = 1.0f;
        style.WindowBorderSize = 1.0f;

        auto& colors = style.Colors;
    }

    // Setup Platform/Renderer backends
    // TODO: move to dedicated style file
    float baseFontSize = 14.0f; // 13.0f is the size of the default font. Change to the font size you use.
    auto font = io.Fonts->AddFontFromFileTTF(Carrot::toString(GetVFS().resolve(IO::VFS::Path("resources/fonts/Roboto-Medium.ttf")).u8string()).c_str(), baseFontSize);

    // from https://github.com/juliettef/IconFontCppHeaders/tree/main/README.md
    float iconFontSize = baseFontSize;// * 2.0f / 3.0f; // FontAwesome fonts need to have their sizes reduced by 2.0f/3.0f in order to align correctly

    // merge in icons from Font Awesome
    static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };
    ImFontConfig icons_config;
    icons_config.MergeMode = true;
    icons_config.PixelSnapH = true;
    icons_config.GlyphMinAdvanceX = iconFontSize;
    io.Fonts->AddFontFromFileTTF( Carrot::toString(GetVFS().resolve(IO::VFS::Path("resources/fonts/Font Awesome 5 Free-Solid-900.otf")).u8string()).c_str(),
                                  iconFontSize, &icons_config, icons_ranges );

    io.Fonts->Build();

    imGuiBackend.initResources();
}

void Carrot::VulkanRenderer::initImGuiPass(const vk::RenderPass& renderPass) {
    // TODO: remove
}

void Carrot::VulkanRenderer::shutdownImGui() {
    imGuiBackend.cleanup();
}

void Carrot::VulkanRenderer::onSwapchainImageCountChange(std::size_t newCount) {
    imGuiBackend.onSwapchainImageCountChange(newCount);
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
    for(const auto& [name, pipePtrPtr]: GetAssetServer().pipelines.snapshot()) {
        (*pipePtrPtr)->onSwapchainImageCountChange(newCount);
    }
}

void Carrot::VulkanRenderer::onSwapchainSizeChange(Window& window, int newWidth, int newHeight) {
    boundTextures.clear();
    raytracer->onSwapchainSizeChange(window, newWidth, newHeight);
    gBuffer->onSwapchainSizeChange(window, newWidth, newHeight);
    materialSystem.onSwapchainSizeChange(window, newWidth, newHeight);
    lighting.onSwapchainSizeChange(window, newWidth, newHeight);
    if(asBuilder) {
        asBuilder->onSwapchainSizeChange(window, newWidth, newHeight);
    }
    for(const auto& [name, pipePtrPtr]: GetAssetServer().pipelines.snapshot()) {
        (*pipePtrPtr)->onSwapchainSizeChange(window, newWidth, newHeight);
    }
}

void Carrot::VulkanRenderer::preFrame(const Render::Context& renderContext) {
    ASSERT_RENDER_THREAD();
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
    ASSERT_RENDER_THREAD();
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
    ASSERT_NOT_RENDER_THREAD();
    beforeFrameCommands.push_back(command);
}
void Carrot::VulkanRenderer::afterFrameCommand(const CommandBufferConsumer& command) {
    ASSERT_NOT_RENDER_THREAD();
    afterFrameCommands.push_back(command);
}

void Carrot::VulkanRenderer::bindSampler(Carrot::Pipeline& pipeline, const Carrot::Render::Context& frame, const vk::Sampler& samplerToBind, std::uint32_t setID, std::uint32_t bindingID) {
    if(boundSamplers[{pipeline, frame.swapchainIndex, setID, bindingID}] == samplerToBind) {
        return;
    }
    boundSamplers[{pipeline, frame.swapchainIndex, setID, bindingID}] = samplerToBind;
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
    if(boundAS[{pipeline, frame.swapchainIndex, setID, bindingID}] == as.getVulkanAS()) {
        return;
    }
    boundAS[{pipeline, frame.swapchainIndex, setID, bindingID}] = as.getVulkanAS();
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
    if(boundBuffers[{pipeline, frame.swapchainIndex, setID, bindingID}] == view.asBufferInfo()) {
        return;
    }
    boundBuffers[{pipeline, frame.swapchainIndex, setID, bindingID}] = view.asBufferInfo();
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
    if(boundBuffers[{pipeline, frame.swapchainIndex, setID, bindingID}] == view.asBufferInfo()) {
        return;
    }
    boundBuffers[{pipeline, frame.swapchainIndex, setID, bindingID}] = view.asBufferInfo();
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
    if(boundStorageImages[{pipeline, frame.swapchainIndex, setID, bindingID, textureLayout}] == textureToBind.getVulkanImage()) {
        return;
    }
    boundStorageImages[{pipeline, frame.swapchainIndex, setID, bindingID, textureLayout}] = textureToBind.getVulkanImage();
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
    if(boundTextures[{pipeline, frame.swapchainIndex, setID, bindingID, textureLayout}] == textureToBind.getVulkanImage()) {
        return;
    }
    boundTextures[{pipeline, frame.swapchainIndex, setID, bindingID, textureLayout}] = textureToBind.getVulkanImage();
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

void Carrot::VulkanRenderer::unbindSampler(Carrot::Pipeline& pipeline, const Carrot::Render::Context& frame, std::uint32_t setID, std::uint32_t bindingID) {
    bindSampler(pipeline, frame, VK_NULL_HANDLE, setID, bindingID);
}

void Carrot::VulkanRenderer::unbindTexture(Pipeline& pipeline, const Render::Context& frame, std::uint32_t setID, std::uint32_t bindingID, std::uint32_t arrayIndex) {
    ZoneScoped;
    BindingKey k { pipeline, frame.swapchainIndex, setID, bindingID };
    std::erase_if(boundTextures, [&](const auto& pair) {
        return pair.first == k; // leave out layout by design
    });
    auto descriptorSet = pipeline.getDescriptorSets(frame, setID)[frame.swapchainIndex];

    vk::DescriptorImageInfo imageInfo {
            .sampler = VK_NULL_HANDLE,
            .imageView = VK_NULL_HANDLE,
    };

    vk::DescriptorType descType = vk::DescriptorType::eSampledImage;
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

void Carrot::VulkanRenderer::unbindStorageImage(Pipeline& pipeline, const Render::Context& frame, std::uint32_t setID, std::uint32_t bindingID, std::uint32_t arrayIndex) {
    ZoneScoped;
    BindingKey k { pipeline, frame.swapchainIndex, setID, bindingID };
    std::erase_if(boundStorageImages, [&](const auto& pair) {
        return pair.first == k; // leave out layout by design
    });
    auto descriptorSet = pipeline.getDescriptorSets(frame, setID)[frame.swapchainIndex];

    vk::DescriptorImageInfo imageInfo {
            .sampler = VK_NULL_HANDLE,
            .imageView = VK_NULL_HANDLE,
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

void Carrot::VulkanRenderer::unbindAccelerationStructure(Pipeline& pipeline, const Render::Context& frame, std::uint32_t setID, std::uint32_t bindingID) {
    ZoneScoped;
    BindingKey k { pipeline, frame.swapchainIndex, setID, bindingID };
    std::erase_if(boundAS, [&](const auto& pair) {
        return pair.first == k; // leave out layout by design
    });
    auto descriptorSet = pipeline.getDescriptorSets(frame, setID)[frame.swapchainIndex];

    vk::AccelerationStructureKHR nullAS = VK_NULL_HANDLE;
    vk::WriteDescriptorSetAccelerationStructureKHR  asInfo {
            .accelerationStructureCount = 1,
            .pAccelerationStructures = &nullAS,
    };

    vk::DescriptorType descType = vk::DescriptorType::eAccelerationStructureKHR;

    std::array<vk::WriteDescriptorSet, 1> writeAS {
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
        driver.getLogicalDevice().updateDescriptorSets(writeAS, {});
    }
}

void Carrot::VulkanRenderer::unbindBuffer(Pipeline& pipeline, const Render::Context& frame, std::uint32_t setID, std::uint32_t bindingID) {
    ZoneScoped;
    BindingKey k { pipeline, frame.swapchainIndex, setID, bindingID };
    std::erase_if(boundBuffers, [&](const auto& pair) {
        return pair.first == k;
    });
    auto descriptorSet = pipeline.getDescriptorSets(frame, setID)[frame.swapchainIndex];

    vk::DescriptorBufferInfo bufferInfo {
            .buffer = VK_NULL_HANDLE,
            .offset = 0,
            .range = VK_WHOLE_SIZE,
    };

    vk::DescriptorType descType = vk::DescriptorType::eStorageBuffer;
    std::array<vk::WriteDescriptorSet, 1> writeTexture {
            vk::WriteDescriptorSet {
                    .dstSet = descriptorSet,
                    .dstBinding = bindingID,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = descType,
                    .pBufferInfo = &bufferInfo,
            }
    };
    {
        ZoneScopedN("driver.getLogicalDevice().updateDescriptorSets");
        driver.getLogicalDevice().updateDescriptorSets(writeTexture, {});
    }
}

void Carrot::VulkanRenderer::unbindUniformBuffer(Pipeline& pipeline, const Render::Context& frame, std::uint32_t setID, std::uint32_t bindingID) {
    ZoneScoped;
    BindingKey k { pipeline, frame.swapchainIndex, setID, bindingID };
    std::erase_if(boundBuffers, [&](const auto& pair) {
        return pair.first == k;
    });
    auto descriptorSet = pipeline.getDescriptorSets(frame, setID)[frame.swapchainIndex];

    vk::DescriptorBufferInfo bufferInfo {
            .buffer = VK_NULL_HANDLE,
    };

    vk::DescriptorType descType = vk::DescriptorType::eUniformBuffer;
    std::array<vk::WriteDescriptorSet, 1> writeTexture {
            vk::WriteDescriptorSet {
                    .dstSet = descriptorSet,
                    .dstBinding = bindingID,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = descType,
                    .pBufferInfo = &bufferInfo,
            }
    };
    {
        ZoneScopedN("driver.getLogicalDevice().updateDescriptorSets");
        driver.getLogicalDevice().updateDescriptorSets(writeTexture, {});
    }
}

void Carrot::VulkanRenderer::newFrame() {
    ZoneScoped;
    ASSERT_NOT_RENDER_THREAD();

    {
        ZoneScopedN("ImGui backend new frame");
        imGuiBackend.newFrame();
    }

    for(auto& resource : deferredCarrotBufferDestructions) {
        resource.tickDown();
    }

    for(auto it = deferredCarrotBufferDestructions.begin(); it != deferredCarrotBufferDestructions.end();) {
        if(it->isReadyForDestruction()) {
            Carrot::Buffer* buf = it->resource;
            delete buf;
            it = deferredCarrotBufferDestructions.erase(it);
        } else {
            it++;
        }
    }

    frameCount++;


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
    {
        ZoneScopedN("Waiting on tasks required before starting the frame");
        mustBeDoneByNextFrameCounter.busyWait();
    }

    // TODO: implement via asset reload system
    // reloaded shaders -> pipeline recreation -> need to rebind descriptor
    if(GetAssetServer().TMLhasReloadedShaders()) {
        boundBuffers.clear();
        boundTextures.clear();
        boundStorageImages.clear();
        boundSamplers.clear();
        boundAS.clear();
    }

    Async::LockGuard lk { threadRegistrationLock };

    const std::size_t currentIndex = getCurrentBufferPointerForMain();
    Async::Counter prepareThreadRenderPackets;
    for (auto& pair : perThreadPacketStorage.snapshot()) {
        TaskDescription task {
            .name = "Reset thread local render packet storage",
            .task = [pair, currentIndex](TaskHandle&) {
                auto& ppArray = pair.second;
                (*(*ppArray))[currentIndex].beginFrame();
            },
            .joiner = &prepareThreadRenderPackets,
        };
        GetTaskScheduler().schedule(std::move(task), TaskScheduler::FrameParallelWork);
    }
    singleFrameAllocator.newFrame(renderContext.swapchainIndex);

    prepareThreadRenderPackets.busyWait();
    if(glfwGetKey(GetEngine().getMainWindow().getGLFWPointer(), GLFW_KEY_F9) == GLFW_PRESS) {
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

void Carrot::VulkanRenderer::startRecord(std::uint8_t frameIndex, const Carrot::Render::Context& renderContext) {
    ZoneScoped;
    ASSERT_NOT_RENDER_THREAD();

    {
        ZoneScopedN("ImGui Render");
        Console::instance().renderToImGui(getEngine());
        ImGui::Render();
        imGuiBackend.render(renderContext, GetEngine().getMainWindow().getWindowID(), ImGui::GetDrawData());

        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault(nullptr, (void*)&renderContext.swapchainIndex);
    }

    // wait for previous frame
    waitForRenderToComplete();
    recordingFrameIndex = frameIndex;

    // swap double-buffered data
    {
        recordingRenderContext.copyFrom(renderContext);
        bufferPointer = 1 - bufferPointer;
        renderData.perDrawData.clear();
        renderData.perDrawOffsets.clear();

        renderData.perDrawOffsetCount = mainData.perDrawOffsetCount.load();
        mainData.perDrawOffsetCount = 0;
        renderData.perDrawElementCount = mainData.perDrawElementCount.load();
        mainData.perDrawElementCount = 0;
    }

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

    const std::size_t currentIndex = getCurrentBufferPointerForRender();
    for(const auto& [threadID, packets] : snapshot) {
        {
            ZoneScopedN("Move thread local packets to global bins");
            if(debugRender) {
                std::size_t packetCount = packets->unsorted.size();
                ImGui::Text("%llu packets from thread", packetCount);
                totalPacketCount += packetCount;
            }
            for(auto& p : packets->unsorted[currentIndex]) {
                placeInBin(std::move(p));
            }
        }
        TaskDescription task {
                .name = "Cleanup thread local render packets",
                .task = [pPackets = packets, currentIndex](TaskHandle&) {
                    std::size_t previousCapacity = pPackets->unsorted[currentIndex].capacity();
                    {
                        ZoneScopedN("thread local packets.clear()");
                        pPackets->unsorted[currentIndex].clear();
                    }
                    {
                        ZoneScopedN("thread local packets.reserve(previousCapacity)");
                        pPackets->unsorted[currentIndex].reserve(previousCapacity);
                    }
                },
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
    packetBins.clear();

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
    renderThreadReady.increment(); // render thread has started working
    renderThreadKickoff.decrement(); // tell render thread to start working
}

void Carrot::VulkanRenderer::recordImGuiPass(vk::CommandBuffer& cmds, vk::RenderPass renderPass, const Carrot::Render::Context& renderContext) {
    imGuiBackend.record(cmds, renderPass, renderContext);
}

void Carrot::VulkanRenderer::waitForRenderToComplete() {
    renderThreadReady.sleepWait();
}

void Carrot::VulkanRenderer::onFrame(const Carrot::Render::Context& renderContext) {
    ZoneScoped;

    {
        static DebugBufferObject obj{};
        auto& buffer = debugBuffers[renderContext.swapchainIndex];

        static std::uint32_t gBufferDebugType = DEBUG_GBUFFER_DISABLED;

        if(ShowGBuffer && renderContext.pViewport == &GetEngine().getMainViewport()) {
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

Carrot::Render::ImGuiBackend& Carrot::VulkanRenderer::getImGuiBackend() {
    return imGuiBackend;
}

Carrot::Render::MaterialSystem& Carrot::VulkanRenderer::getMaterialSystem() {
    return materialSystem;
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

void Carrot::VulkanRenderer::preallocatePerDrawBuffers(const Carrot::Render::Context& renderContext) {
    ASSERT_RENDER_THREAD();
    const std::size_t elementSize = sizeof(GBufferDrawData);
    const std::size_t requiredStorage = renderData.perDrawElementCount * elementSize;
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
    const std::size_t offsetRequiredStorage = renderData.perDrawOffsetCount * alignedElementSize;

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
}

void Carrot::VulkanRenderer::updatePerDrawBuffers(const Carrot::Render::Context& renderContext) {
    ZoneScoped;
    const std::size_t elementSize = sizeof(GBufferDrawData);
    const std::size_t requiredStorage = renderData.perDrawData.size() * elementSize;

    // perDrawElementCount should accurately predict how many elements perDrawData has (same for perDrawOffsetCount)
    verify(renderData.perDrawData.size() == renderData.perDrawElementCount, "perDrawData.size() != perDrawElementCount.load(), this is a programming error!");
    verify(renderData.perDrawOffsets.size() == renderData.perDrawOffsetCount, "perDrawData.size() != perDrawElementCount.load(), this is a programming error!");
    if(requiredStorage == 0)
        return;

    const std::size_t frameIndex = renderContext.swapchainIndex;

    const std::size_t alignedElementSize = Carrot::Math::alignUp(sizeof(std::uint32_t), GetVulkanDriver().getPhysicalDeviceLimits().minUniformBufferOffsetAlignment);

    std::size_t index = 0;
    std::vector<std::pair<std::size_t, std::span<const std::uint32_t>>> copyTargets;
    copyTargets.reserve(renderData.perDrawOffsets.size());
    for(const auto& element : renderData.perDrawOffsets) {
        const std::size_t offset = index * alignedElementSize;

        copyTargets.emplace_back(offset, std::span{&element, 1});
        index++;
    }

    // TODO: async upload
    perDrawOffsetBuffers[frameIndex]->stageUploadWithOffsets(std::span { copyTargets });
    perDrawBuffers[frameIndex]->stageUploadWithOffset(0ull, renderData.perDrawData.data(), renderData.perDrawData.size() * sizeof(GBufferDrawData));
}

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
            .maxSets = static_cast<uint32_t>(getSwapchainImageCount()),
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
    getMaterialSystem().init();

    std::unique_ptr<Image> cubeMap = std::make_unique<Image>(
            GetVulkanDriver(),
            vk::Extent3D{1,1,1},
            vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
            vk::Format::eR8G8B8A8Unorm,
            GetVulkanDriver().createGraphicsAndTransferFamiliesSet(),
            vk::ImageCreateFlagBits::eCubeCompatible,
            vk::ImageType::e2D,
            6);

    defaultTexture = getOrCreateTexture("default.png");

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

void Carrot::VulkanRenderer::deferDestroy(const std::string& name, Carrot::Buffer* resource) {
    deferredCarrotBufferDestructions.push_back(std::move(DeferredCarrotBufferDestruction(name, std::move(resource))));
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
    return defaultTexture;
}

Carrot::Render::Texture::Ref Carrot::VulkanRenderer::getBlackCubeMapTexture() {
    return blackCubeMapTexture;
}

void Carrot::VulkanRenderer::recordPassPackets(Carrot::Render::PassEnum packetPass, vk::RenderPass pass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) {
    ZoneScoped;
    ASSERT_RENDER_THREAD();

    if(blinkTime > 0.0) {
        return;
    }

    Carrot::Render::Viewport* viewport = renderContext.pViewport;
    verify(viewport, "Viewport cannot be null");

    auto packets = getRenderPackets(viewport, packetPass);
    const Carrot::Render::Packet* previousPacket = nullptr;
    for(const auto& p : packets) {
        p.record(pass, renderContext, commands, previousPacket);
        previousPacket = &p;
    }
}

void Carrot::VulkanRenderer::recordOpaqueGBufferPass(vk::RenderPass pass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) {
    recordPassPackets(Render::PassEnum::OpaqueGBuffer, pass, renderContext, commands);
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
            .frameWidth = renderContext.pViewport->getWidth(),
            .frameHeight = renderContext.pViewport->getHeight(),

            .hasTLAS = useRaytracingVersion ? getASBuilder().getTopLevelAS(renderContext) != nullptr : false
    };

    forwardRenderingFrameInfoBuffer->directUpload(&frameInfo, sizeof(frameInfo));
    bindUniformBuffer(*forwardPipeline, renderContext, forwardRenderingFrameInfoBuffer->getWholeView(), 5, 1);
    if(useRaytracingVersion) {
        auto* pTLAS = getASBuilder().getTopLevelAS(renderContext);
        if(pTLAS) {
            bindAccelerationStructure(*forwardPipeline, renderContext, *pTLAS, 5, 2);
            bindTexture(*forwardPipeline, renderContext, *getMaterialSystem().getBlueNoiseTextures()[renderContext.swapchainIndex % Render::BlueNoiseTextureCount]->texture, 5, 3, nullptr);
            bindBuffer(*forwardPipeline, renderContext, getASBuilder().getGeometriesBuffer(renderContext), 5, 4);
            bindBuffer(*forwardPipeline, renderContext, getASBuilder().getInstancesBuffer(renderContext), 5, 5);
        }
    }

    recordPassPackets(Render::PassEnum::TransparentGBuffer, pass, renderContext, commands);
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

void Carrot::VulkanRenderer::renderSphere(const Carrot::Render::Context& renderContext, const glm::mat4& transform, float radius, const glm::vec4& color, const Carrot::UUID& objectID) {
    renderModel(*unitSphereModel, renderContext, glm::scale(transform, glm::vec3(radius)), color, objectID);
}

void Carrot::VulkanRenderer::renderCapsule(const Carrot::Render::Context& renderContext, const glm::mat4& transform, float radius, float height, const glm::vec4& color, const Carrot::UUID& objectID) {
    renderModel(*unitCapsuleModel, renderContext, glm::scale(transform, glm::vec3(radius, radius, height)), color, objectID);
}

void Carrot::VulkanRenderer::renderCuboid(const Carrot::Render::Context& renderContext, const glm::mat4& transform, const glm::vec3& halfExtents, const glm::vec4& color, const Carrot::UUID& objectID) {
    renderModel(*unitCubeModel, renderContext, transform * glm::scale(glm::mat4(1.0f), halfExtents * 2.0f), color, objectID);
}

void Carrot::VulkanRenderer::renderWireframeSphere(const Carrot::Render::Context& renderContext, const glm::mat4& transform, float radius, const glm::vec4& color, const Carrot::UUID& objectID) {
    renderWireframeModel(*unitSphereModel, renderContext, glm::scale(transform, glm::vec3(radius)), color, objectID);
}

void Carrot::VulkanRenderer::renderWireframeCapsule(const Carrot::Render::Context& renderContext, const glm::mat4& transform, float radius, float height, const glm::vec4& color, const Carrot::UUID& objectID) {
    renderWireframeModel(*unitCapsuleModel, renderContext, glm::scale(transform, glm::vec3(radius, radius, height)), color, objectID);
}

void Carrot::VulkanRenderer::renderWireframeCuboid(const Carrot::Render::Context& renderContext, const glm::mat4& transform, const glm::vec3& halfExtents, const glm::vec4& color, const Carrot::UUID& objectID) {
    renderWireframeModel(*unitCubeModel, renderContext, transform * glm::scale(glm::mat4(1.0f), halfExtents * 2.0f), color, objectID);
}

void Carrot::VulkanRenderer::render3DArrow(const Carrot::Render::Context& renderContext, const glm::mat4& transform, const glm::vec4& color, const Carrot::UUID& objectID) {
    renderModel(*debugArrowModel, renderContext, transform, color, objectID);
}

void Carrot::VulkanRenderer::renderModel(const Carrot::Model& model, const Carrot::Render::Context& renderContext, const glm::mat4& transform, const glm::vec4& color, const Carrot::UUID& objectID) {
    Render::Packet& packet = GetRenderer().makeRenderPacket(Carrot::Render::PassEnum::OpaqueGBuffer, renderContext);
    Carrot::GBufferDrawData data;
    data.materialIndex = whiteMaterial->getSlot();

    packet.useMesh(*model.getStaticMeshes()[0]); // TODO: find a better way to load individual meshes
    packet.pipeline = gBufferPipeline;

    packet.addPerDrawData({&data, 1});

    Carrot::InstanceData instance;
    instance.uuid = objectID;
    instance.color = color;
    instance.transform = transform;
    packet.useInstance(instance);
    render(packet);
}

void Carrot::VulkanRenderer::renderWireframeModel(const Carrot::Model& model, const Carrot::Render::Context& renderContext, const glm::mat4& transform, const glm::vec4& color, const Carrot::UUID& objectID) {
    Render::Packet& packet = GetRenderer().makeRenderPacket(Carrot::Render::PassEnum::OpaqueGBuffer, renderContext);
    Carrot::GBufferDrawData data;
    data.materialIndex = whiteMaterial->getSlot();

    packet.useMesh(*model.getStaticMeshes()[0]); // TODO: find a better way to load individual meshes
    packet.pipeline = wireframeGBufferPipeline;

    packet.addPerDrawData({&data, 1});

    Carrot::InstanceData instance;
    instance.uuid = objectID;
    instance.color = color;
    instance.transform = transform;
    packet.useInstance(instance);
    render(packet);
}

std::uint32_t Carrot::VulkanRenderer::uploadPerDrawData(const std::span<GBufferDrawData>& drawData) {
    ASSERT_RENDER_THREAD();
    const std::size_t alignedElementSize = Math::alignUp(sizeof(std::uint32_t), driver.getPhysicalDeviceLimits().minUniformBufferOffsetAlignment);

    std::size_t offset = renderData.perDrawOffsets.size() * alignedElementSize;
    renderData.perDrawOffsets.emplace_back(static_cast<std::uint32_t>(renderData.perDrawData.size()));

    for(const auto& data : drawData) {
        renderData.perDrawData.emplace_back(data);
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

Carrot::Render::Packet& Carrot::VulkanRenderer::makeRenderPacket(Render::PassEnum pass, const Render::Context& renderContext, std::source_location location) {
    return makeRenderPacket(pass, *renderContext.pViewport, location);
}

Carrot::Render::Packet& Carrot::VulkanRenderer::makeRenderPacket(Render::PassEnum pass, Render::Viewport& viewport, std::source_location location) {
    return (*threadLocalPacketStorage)[getCurrentBufferPointerForMain()].make(pass, &viewport, location);
}

void Carrot::VulkanRenderer::render(const Render::Packet& packet) {
    ASSERT_NOT_RENDER_THREAD();
    ZoneScopedN("Queue RenderPacket");
    packet.validate();
    verify(threadLocalRenderPackets != nullptr, "Current thread must have been registered via VulkanRenderer::makeCurrentThreadRenderCapable()");
    threadLocalRenderPackets->unsorted[getCurrentBufferPointerForMain()].emplace_back(packet);

    if(!packet.perDrawData.empty()) {
        mainData.perDrawElementCount += packet.perDrawData.size() / sizeof(GBufferDrawData);
        mainData.perDrawOffsetCount++;
    }
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

const Carrot::BufferView Carrot::VulkanRenderer::getNullBufferInfo() const {
    return nullBuffer->getWholeView();
}

void Carrot::VulkanRenderer::makeCurrentThreadRenderCapable() {
    Async::LockGuard lk { threadRegistrationLock };
    auto& packetsEntry = threadRenderPackets.getOrCompute(std::this_thread::get_id(), [](){ return ThreadPackets{}; });
    auto& storageEntry = perThreadPacketStorage.getOrCompute(std::this_thread::get_id(),
     []()
     {
        return std::make_unique<std::array<Render::PacketContainer, 2>>();
     });
    threadLocalRenderPackets = &packetsEntry;
    threadLocalPacketStorage = storageEntry.get();
}

std::int8_t Carrot::VulkanRenderer::getCurrentBufferPointerForMain() {
    return bufferPointer;
};

std::int8_t Carrot::VulkanRenderer::getCurrentBufferPointerForRender() {
    return 1 - bufferPointer;
}

void Carrot::VulkanRenderer::renderThreadProc() {
    renderThreadReady.decrement();

    while(true) {
        renderThreadKickoff.sleepWait();
        if(!running) {
            break;
        }

        auto timeStart = std::chrono::steady_clock::now();
        materialSystem.beginFrame(recordingRenderContext); // called here because new material may have been created during the frame
        lighting.beginFrame(recordingRenderContext);
        preallocatePerDrawBuffers(recordingRenderContext);

        GetEngine().recordMainCommandBufferAndPresent(recordingFrameIndex, recordingRenderContext);

        auto timeElapsed = std::chrono::steady_clock::now() - timeStart;
        latestRecordTime = std::chrono::duration<float>(timeElapsed).count();
        renderThreadKickoff.increment(); // render thread is waiting for something to do
        renderThreadReady.decrement(); // render thread has finished working
    }
}

float Carrot::VulkanRenderer::getLastRecordDuration() const {
    return latestRecordTime;
}