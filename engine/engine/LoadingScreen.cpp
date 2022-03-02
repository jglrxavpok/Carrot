//
// Created by jglrxavpok on 17/03/2021.
//

#include "LoadingScreen.h"
#include "render/resources/Buffer.h"
#include "render/resources/Pipeline.h"
#include "render/resources/Mesh.h"
#include "engine/Engine.h"

Carrot::LoadingScreen::LoadingScreen(Engine& engine): engine(engine) {
    loadingImage = Image::fromFile(engine.getVulkanDriver(), "resources/splash.png");

    auto& device = engine.getLogicalDevice();

    // prepare pipeline
    vk::AttachmentDescription output {
            .format = vk::Format::eB8G8R8A8Unorm,

            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,

            .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
            .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,

            .initialLayout = vk::ImageLayout::eUndefined,
            .finalLayout = vk::ImageLayout::ePresentSrcKHR,
    };

    vk::AttachmentReference outputRef {
            .attachment = 0,
            .layout = vk::ImageLayout::eColorAttachmentOptimal,
    };

    vk::SubpassDescription subpass {
            .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
            .inputAttachmentCount = 0,
            .colorAttachmentCount = 1,
            .pColorAttachments = &outputRef,
    };

    vk::UniqueRenderPass blitRenderPass = device.createRenderPassUnique(vk::RenderPassCreateInfo {
            .attachmentCount = 1,
            .pAttachments = &output,

            .subpassCount = 1,
            .pSubpasses = &subpass,
    });

    auto& driver = engine.getVulkanDriver();

    vk::UniqueSemaphore imageAvailable = device.createSemaphoreUnique(vk::SemaphoreCreateInfo {

    });

    auto imageIndex = device.acquireNextImageKHR(engine.getVulkanDriver().getSwapchain(), UINT64_MAX, *imageAvailable, nullptr).value;

    auto& swapchainExtent = driver.getWindowFramebufferExtent(); // TODO: will need to be changed for VR compatible loading screens
    auto swapchainView = engine.getVulkanDriver().getSwapchainTextures()[imageIndex]->getView();
    vk::UniqueFramebuffer framebuffer = device.createFramebufferUnique(vk::FramebufferCreateInfo {
        .renderPass = *blitRenderPass,
        .attachmentCount = 1,
        .pAttachments = &swapchainView,

        .width = swapchainExtent.width,
        .height = swapchainExtent.height,

        .layers = 1,
    });

    auto pipeline = Pipeline(engine.getVulkanDriver(), "resources/pipelines/blit.json");

    auto loadingImageView = loadingImage->createImageView();
    vk::DescriptorImageInfo loadingImageInfo {
            .sampler = driver.getLinearSampler(),
            .imageView = *loadingImageView,
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
    };

    auto renderContext = GetEngine().newRenderContext(imageIndex, GetEngine().getMainViewport());
    auto set = pipeline.getDescriptorSets(renderContext, 0)[imageIndex];
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

    PrepareVulkanTracy(engine.tracyCtx[imageIndex], cmds);

    auto quad = Mesh(driver,
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

    driver.updateViewportAndScissor(cmds, swapchainExtent);

    vk::ClearValue color{std::array{1.0f,1.0f,0.0f,1.0f}};
    cmds.beginRenderPass(vk::RenderPassBeginInfo {
            .renderPass = *blitRenderPass,
            .framebuffer = *framebuffer,

            .renderArea = {
                    .offset = vk::Offset2D{0, 0},
                    .extent = swapchainExtent,
            },

            .clearValueCount = 1,
            .pClearValues = &color,
    }, vk::SubpassContents::eInline);

    pipeline.bind(*blitRenderPass, engine.newRenderContext(0, engine.getMainViewport()), cmds);
    quad.bind(cmds);
    quad.draw(cmds);

    cmds.endRenderPass();

    cmds.end();

    std::vector<vk::PipelineStageFlags> waitStages = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
    vk::SubmitInfo submitInfo {
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = nullptr,

            .pWaitDstStageMask = waitStages.data(),

            .commandBufferCount = 1,
            .pCommandBuffers = &cmds,

            .signalSemaphoreCount = 0,
            .pSignalSemaphores = nullptr,
    };

    GetVulkanDriver().submitGraphics(submitInfo, nullptr);
    engine.getGraphicsQueue().waitIdle();

    vk::SwapchainKHR swapchains[] = { engine.getVulkanDriver().getSwapchain() };

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