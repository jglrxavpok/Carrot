//
// Created by jglrxavpok on 17/03/2021.
//

#include "LoadingScreen.h"
#include "render/resources/Buffer.h"
#include "render/resources/Pipeline.h"
#include "render/resources/SingleMesh.h"
#include "engine/Engine.h"

Carrot::LoadingScreen::LoadingScreen(Engine& engine): engine(engine) {
    loadingImage = Image::fromFile(engine.getVulkanDriver(), "resources/splash.png");

    auto& device = engine.getLogicalDevice();

    auto& driver = engine.getVulkanDriver();

    vk::UniqueSemaphore imageAvailable = device.createSemaphoreUnique(vk::SemaphoreCreateInfo {

    });

    auto imageIndex = device.acquireNextImageKHR(engine.getMainWindow().getSwapchain(), UINT64_MAX, *imageAvailable, nullptr).value;

    auto& swapchainExtent = engine.getMainWindow().getFramebufferExtent(); // TODO: will need to be changed for VR compatible loading screens
    Render::Texture::Ref swapchainTexture = engine.getMainWindow().getSwapchainTexture(imageIndex);
    auto swapchainView = swapchainTexture->getView();
    auto pipeline = Pipeline(engine.getVulkanDriver(), "resources/pipelines/blit.pipeline");

    auto loadingImageView = loadingImage->createImageView();
    vk::DescriptorImageInfo loadingImageInfo {
            .sampler = driver.getLinearSampler(),
            .imageView = *loadingImageView,
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
    };

    const u8 frameIndex = 0;
    auto renderContext = GetEngine().newRenderContext(frameIndex, imageIndex, GetEngine().getMainViewport());
    auto set = pipeline.getDescriptorSets(renderContext, 0)[frameIndex];
    vk::WriteDescriptorSet writeLoadingImage {
            .dstSet = set,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eSampledImage,
            .pImageInfo = &loadingImageInfo,
    };
    device.updateDescriptorSets(writeLoadingImage, {});

    // allocate command buffer
    vk::CommandBufferAllocateInfo allocationInfo {
            .commandPool = engine.getGraphicsCommandPool(),
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1,
    };
    vk::CommandBuffer cmds = device.allocateCommandBuffers(allocationInfo)[0];

    // begin recording
    vk::CommandBufferBeginInfo beginInfo {
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
    };
    cmds.begin(beginInfo);

    PrepareVulkanTracy(engine.tracyCtx[frameIndex], cmds);

    auto quad = SingleMesh(
                     std::vector<ScreenSpaceVertex> {
                               { { -1, -1} },
                               { { 1, -1} },
                               { { 1, 1} },
                               { { -1, 1} },
                     },
                     std::vector<uint32_t> {
                               2,1,0,
                               3,2,0,
                     });

    quad.name("Fullscreen quad (loading screen)");

    driver.updateViewportAndScissor(cmds, swapchainExtent);

    vk::ClearValue color{std::array{1.0f,1.0f,0.0f,1.0f}};

    vk::RenderingAttachmentInfo colorAttachmentInfo {
        .imageView = swapchainView,
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .resolveMode = vk::ResolveModeFlagBits::eNone,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .clearValue = color,
    };

    cmds.beginRendering(vk::RenderingInfo {
            .renderArea = {
                    .offset = vk::Offset2D{0, 0},
                    .extent = swapchainExtent,
            },
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachmentInfo,
    });

        RenderingPipelineCreateInfo createInfo {
                .colorAttachments = {swapchainTexture->getImage().getFormat()}
        };
    pipeline.bind(createInfo, engine.newRenderContext(frameIndex, imageIndex, engine.getMainViewport()), cmds);
    quad.bind(cmds);
    quad.draw(cmds);

    cmds.endRendering();

    cmds.end();
    vk::CommandBufferSubmitInfo commandInfo {
            .commandBuffer = cmds,
    };
    vk::SubmitInfo2 submitInfo {
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = &commandInfo,
    };

    GetVulkanDriver().submitGraphics(submitInfo, nullptr);
    engine.getGraphicsQueue().waitIdle();

    vk::SwapchainKHR swapchains[] = { engine.getMainWindow().getSwapchain() };

    vk::PresentInfoKHR presentInfo{
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &(*imageAvailable),

            .swapchainCount = 1,
            .pSwapchains = swapchains,
            .pImageIndices = &imageIndex,
            .pResults = nullptr,
    };

    static_cast<void>(engine.getPresentQueue().presentKHR(presentInfo));
    engine.getPresentQueue().waitIdle();
    device.freeCommandBuffers(engine.getGraphicsCommandPool(), cmds);
}