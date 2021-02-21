//
// Created by jglrxavpok on 28/12/2020.
//

#include "GBuffer.h"
#include "render/Pipeline.h"
#include "render/Mesh.h"
#include "render/Image.h"
#include "RenderPasses.h"

Carrot::GBuffer::GBuffer(Carrot::Engine& engine, Carrot::RayTracer& raytracer): engine(engine), raytracer(raytracer) {
    screenQuadMesh = make_unique<Mesh>(engine,
                                       vector<ScreenSpaceVertex>{
                                               { { -1, -1} },
                                               { { 1, -1} },
                                               { { 1, 1} },
                                               { { -1, 1} },
                                       },
                                       vector<uint32_t>{
                                               2,1,0,
                                               3,2,0,
                                       });

    albedoImages.resize(engine.getSwapchainImageCount());
    albedoImageViews.resize(engine.getSwapchainImageCount());

    depthImages.resize(engine.getSwapchainImageCount());
    depthStencilImageViews.resize(engine.getSwapchainImageCount());
    depthOnlyImageViews.resize(engine.getSwapchainImageCount());

    viewPositionImages.resize(engine.getSwapchainImageCount());
    viewPositionImageViews.resize(engine.getSwapchainImageCount());

    viewNormalImages.resize(engine.getSwapchainImageCount());
    viewNormalImageViews.resize(engine.getSwapchainImageCount());

    for(size_t index = 0; index < engine.getSwapchainImageCount(); index++) {
        // Albedo
        auto swapchainExtent = engine.getSwapchainExtent();
        albedoImages[index] = move(make_unique<Image>(engine,
                                                            vk::Extent3D{swapchainExtent.width, swapchainExtent.height, 1},
                                                            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment,
                                                            vk::Format::eR8G8B8A8Unorm));

        auto view = albedoImages[index]->createImageView();
        albedoImageViews[index] = std::move(view);

        // Depth
        depthImages[index] = move(make_unique<Image>(engine,
                                        vk::Extent3D{swapchainExtent.width, swapchainExtent.height, 1},
                                        vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eInputAttachment,
                                        engine.getDepthFormat()));

        depthStencilImageViews[index] = move(engine.createImageView(depthImages[index]->getVulkanImage(), engine.getDepthFormat(), vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil));
        depthOnlyImageViews[index] = move(engine.createImageView(depthImages[index]->getVulkanImage(), engine.getDepthFormat(), vk::ImageAspectFlagBits::eDepth));

        // View-space positions
        viewPositionImages[index] = move(make_unique<Image>(engine,
                                                      vk::Extent3D{swapchainExtent.width, swapchainExtent.height, 1},
                                                      vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment,
                                                      vk::Format::eR32G32B32A32Sfloat));

        viewPositionImageViews[index] = std::move(viewPositionImages[index]->createImageView(vk::Format::eR32G32B32A32Sfloat));

        // View-space normals
        viewNormalImages[index] = move(make_unique<Image>(engine,
                                                            vk::Extent3D{swapchainExtent.width, swapchainExtent.height, 1},
                                                            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment,
                                                            vk::Format::eR32G32B32A32Sfloat));

        viewNormalImageViews[index] = std::move(viewNormalImages[index]->createImageView(vk::Format::eR32G32B32A32Sfloat));
    }
}

void Carrot::GBuffer::loadResolvePipeline() {
    gResolvePipeline = engine.getOrCreatePipeline("gResolve");

    const size_t imageCount = 6/*albedo+depth+viewPosition+normal+lighting+ui*/;
    vector<vk::WriteDescriptorSet> writes{engine.getSwapchainImageCount()*imageCount};
    vector<vk::DescriptorImageInfo> imageInfo{engine.getSwapchainImageCount()*imageCount};
    for(size_t i = 0; i < engine.getSwapchainImageCount(); i++) {
        // albedo
        auto& albedoInfo = imageInfo[i*imageCount];
        albedoInfo.imageView = *albedoImageViews[i];
        albedoInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        albedoInfo.sampler = *engine.getLinearSampler();

        auto& writeAlbedo = writes[i*imageCount];
        writeAlbedo.descriptorCount = 1;
        writeAlbedo.descriptorType = vk::DescriptorType::eInputAttachment;
        writeAlbedo.pImageInfo = &albedoInfo;
        writeAlbedo.dstSet = gResolvePipeline->getDescriptorSets()[i];
        writeAlbedo.dstArrayElement = 0;
        writeAlbedo.dstBinding = 0;

        // depth
        auto& depthInfo = imageInfo[i*imageCount+1];
        depthInfo.imageView = *depthOnlyImageViews[i];
        depthInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        depthInfo.sampler = *engine.getLinearSampler();

        auto& writeDepth = writes[i*imageCount+1];
        writeDepth.descriptorCount = 1;
        writeDepth.descriptorType = vk::DescriptorType::eInputAttachment;
        writeDepth.pImageInfo = &depthInfo;
        writeDepth.dstSet = gResolvePipeline->getDescriptorSets()[i];
        writeDepth.dstArrayElement = 0;
        writeDepth.dstBinding = 1;

        // view-space positions
        auto& viewPositionInfo = imageInfo[i*imageCount+2];
        viewPositionInfo.imageView = *viewPositionImageViews[i];
        viewPositionInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        viewPositionInfo.sampler = *engine.getLinearSampler();

        auto& writeViewPosition = writes[i*imageCount+2];
        writeViewPosition.descriptorCount = 1;
        writeViewPosition.descriptorType = vk::DescriptorType::eInputAttachment;
        writeViewPosition.pImageInfo = &viewPositionInfo;
        writeViewPosition.dstSet = gResolvePipeline->getDescriptorSets()[i];
        writeViewPosition.dstArrayElement = 0;
        writeViewPosition.dstBinding = 2;

        // normals
        auto& viewNormalInfo = imageInfo[i*imageCount+3];
        viewNormalInfo.imageView = *viewNormalImageViews[i];
        viewNormalInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        viewNormalInfo.sampler = *engine.getLinearSampler();

        auto& writeViewNormal = writes[i*imageCount+3];
        writeViewNormal.descriptorCount = 1;
        writeViewNormal.descriptorType = vk::DescriptorType::eInputAttachment;
        writeViewNormal.pImageInfo = &viewNormalInfo;
        writeViewNormal.dstSet = gResolvePipeline->getDescriptorSets()[i];
        writeViewNormal.dstArrayElement = 0;
        writeViewNormal.dstBinding = 3;

        // raytraced lighting
        auto& lightingNfo = imageInfo[i*imageCount+4];
        lightingNfo.imageView = *raytracer.getLightingImageViews()[i];
        lightingNfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        lightingNfo.sampler = nullptr;

        auto& writeLighting = writes[i*imageCount+4];
        writeLighting.descriptorCount = 1;
        writeLighting.descriptorType = vk::DescriptorType::eSampledImage;
        writeLighting.pImageInfo = &lightingNfo;
        writeLighting.dstSet = gResolvePipeline->getDescriptorSets()[i];
        writeLighting.dstArrayElement = 0;
        writeLighting.dstBinding = 4;

        // ui
        auto& uiInfo = imageInfo[i*imageCount+5];
        uiInfo.imageView = *engine.getUIImageViews()[i];
        uiInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        uiInfo.sampler = nullptr;

        auto& writeUI = writes[i*imageCount+5];
        writeUI.descriptorCount = 1;
        writeUI.descriptorType = vk::DescriptorType::eSampledImage;
        writeUI.pImageInfo = &uiInfo;
        writeUI.dstSet = gResolvePipeline->getDescriptorSets()[i];
        writeUI.dstArrayElement = 0;
        writeUI.dstBinding = 5;
    }
    engine.getLogicalDevice().updateDescriptorSets(writes, {});
}

void Carrot::GBuffer::recordResolvePass(uint32_t frameIndex, vk::CommandBuffer& commandBuffer, vk::CommandBufferInheritanceInfo* inheritance) const {
    vk::CommandBufferBeginInfo beginInfo{
            .flags = vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse,
            .pInheritanceInfo = inheritance,
    };

    commandBuffer.begin(beginInfo);
    {
        engine.updateViewportAndScissor(commandBuffer);
        gResolvePipeline->bind(frameIndex, commandBuffer);
        screenQuadMesh->bind(commandBuffer);
        screenQuadMesh->draw(commandBuffer);
    }
    commandBuffer.end();
}

void Carrot::GBuffer::onSwapchainRecreation() {
    // TODO
}

void Carrot::GBuffer::addFramebufferAttachments(uint32_t frameIndex, vector<vk::ImageView>& attachments) {
    attachments.push_back(*depthOnlyImageViews[frameIndex]);
    attachments.push_back(*albedoImageViews[frameIndex]);
    attachments.push_back(*viewPositionImageViews[frameIndex]);
    attachments.push_back(*viewNormalImageViews[frameIndex]);
}

vk::UniqueRenderPass Carrot::GBuffer::createRenderPass() {
    vk::AttachmentDescription finalColorAttachment {
            .format = engine.getSwapchainImageFormat(),
            .samples = vk::SampleCountFlagBits::e1,

            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,

            .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
            .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,

            .initialLayout = vk::ImageLayout::eUndefined,
            .finalLayout = vk::ImageLayout::ePresentSrcKHR,
    };

    vk::AttachmentReference finalColorAttachmentRef{
            .attachment = 0,
            .layout = vk::ImageLayout::eColorAttachmentOptimal,
    };

    vk::AttachmentDescription depthAttachment{
            .format = engine.getDepthFormat(),
            .samples = vk::SampleCountFlagBits::e1,

            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,

            .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
            .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,

            .initialLayout = vk::ImageLayout::eUndefined,
            .finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
    };

    vk::AttachmentReference depthAttachmentRef{
            .attachment = 1,
            .layout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
    };

    vk::AttachmentReference depthInputAttachmentRef{
            .attachment = 1,
            .layout = vk::ImageLayout::eShaderReadOnlyOptimal,
    };

    vk::AttachmentDescription gBufferColorAttachment {
            .format = vk::Format::eR8G8B8A8Unorm,
            .samples = vk::SampleCountFlagBits::e1,

            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,

            .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
            .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,

            .initialLayout = vk::ImageLayout::eUndefined,
            .finalLayout = vk::ImageLayout::eColorAttachmentOptimal,
    };

    vk::AttachmentReference gBufferColorOutputAttachmentRef{
            .attachment = 2,
            .layout = vk::ImageLayout::eColorAttachmentOptimal,
    };

    vk::AttachmentReference gBufferColorInputAttachmentRef{
            .attachment = 2,
            .layout = vk::ImageLayout::eShaderReadOnlyOptimal,
    };

    vk::AttachmentDescription gBufferViewPositionAttachment {
            .format = vk::Format::eR32G32B32A32Sfloat,
            .samples = vk::SampleCountFlagBits::e1,

            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,

            .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
            .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,

            .initialLayout = vk::ImageLayout::eUndefined,
            .finalLayout = vk::ImageLayout::eColorAttachmentOptimal,
    };

    vk::AttachmentReference gBufferViewPositionOutputAttachmentRef{
            .attachment = 3,
            .layout = vk::ImageLayout::eColorAttachmentOptimal,
    };

    vk::AttachmentReference gBufferViewPositionInputAttachmentRef{
            .attachment = 3,
            .layout = vk::ImageLayout::eShaderReadOnlyOptimal,
    };

    vk::AttachmentDescription gBufferNormalAttachment {
            .format = vk::Format::eR32G32B32A32Sfloat,
            .samples = vk::SampleCountFlagBits::e1,

            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,

            .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
            .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,

            .initialLayout = vk::ImageLayout::eUndefined,
            .finalLayout = vk::ImageLayout::eColorAttachmentOptimal,
    };

    vk::AttachmentReference gBufferNormalOutputAttachmentRef{
            .attachment = 4,
            .layout = vk::ImageLayout::eColorAttachmentOptimal,
    };

    vk::AttachmentReference gBufferNormalInputAttachmentRef{
            .attachment = 4,
            .layout = vk::ImageLayout::eShaderReadOnlyOptimal,
    };

    vk::AttachmentReference outputs[] = {
            gBufferColorOutputAttachmentRef,
            gBufferViewPositionOutputAttachmentRef,
            gBufferNormalOutputAttachmentRef,
    };

    vk::SubpassDescription gBufferSubpass {
            .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
            .inputAttachmentCount = 0,

            .colorAttachmentCount = 3,
            // index in this array is used by `layout(location = 0)` inside shaders
            .pColorAttachments = outputs,
            .pDepthStencilAttachment = &depthAttachmentRef,

            .preserveAttachmentCount = 0,
    };

    vk::AttachmentReference resolveInputs[] = {
            gBufferColorInputAttachmentRef,
            depthInputAttachmentRef,
            gBufferViewPositionInputAttachmentRef,
            gBufferNormalInputAttachmentRef,
    };

    vk::SubpassDescription gResolveSubpass {
            .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
            .inputAttachmentCount = 4,
            .pInputAttachments = resolveInputs,

            .colorAttachmentCount = 1,
            // index in this array is used by `layout(location = 0)` inside shaders
            .pColorAttachments = &finalColorAttachmentRef,
            .pDepthStencilAttachment = nullptr,

            .preserveAttachmentCount = 0,
    };

    vk::SubpassDescription subpasses[] = {
            gBufferSubpass,
            gResolveSubpass,
    };

    vk::SubpassDependency dependencies[] = {
            {
                    .srcSubpass = VK_SUBPASS_EXTERNAL,
                    .dstSubpass = RenderPasses::GBufferSubPassIndex, // gBuffer

                    .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                                    vk::PipelineStageFlagBits::eEarlyFragmentTests,
                    // TODO: .srcAccessMask = 0,

                    .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                                    vk::PipelineStageFlagBits::eEarlyFragmentTests,
                    .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite |
                                     vk::AccessFlagBits::eDepthStencilAttachmentWrite,
            },
            {
                    .srcSubpass = RenderPasses::GBufferSubPassIndex, // gBuffer
                    .dstSubpass = RenderPasses::GResolveSubPassIndex, // gResolve

                    .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                                    vk::PipelineStageFlagBits::eEarlyFragmentTests,
                    // TODO: .srcAccessMask = 0,

                    .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                                    vk::PipelineStageFlagBits::eEarlyFragmentTests,
                    .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
            }
    };

    vector<vk::AttachmentDescription> attachments = {
            finalColorAttachment,
            depthAttachment,
            gBufferColorAttachment,
            gBufferViewPositionAttachment,
            gBufferNormalAttachment,
    };

    vk::RenderPassCreateInfo renderPassInfo{
            .attachmentCount = static_cast<uint32_t>(attachments.size()),
            .pAttachments = attachments.data(),
            .subpassCount = sizeof(subpasses)/sizeof(vk::SubpassDescription),
            .pSubpasses = subpasses,

            .dependencyCount = sizeof(dependencies)/sizeof(vk::SubpassDependency),
            .pDependencies = dependencies,
    };

    return engine.getLogicalDevice().createRenderPassUnique(renderPassInfo, engine.getAllocator());
}
