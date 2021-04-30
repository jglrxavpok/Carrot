//
// Created by jglrxavpok on 28/12/2020.
//

#include "GBuffer.h"
#include "engine/render/resources/Pipeline.h"
#include "engine/render/resources/Mesh.h"
#include "engine/render/resources/Image.h"
#include "engine/RenderPasses.h"

Carrot::GBuffer::GBuffer(Carrot::VulkanRenderer& renderer, Carrot::RayTracer& raytracer): renderer(renderer), raytracer(raytracer) {
    screenQuadMesh = make_unique<Mesh>(renderer.getVulkanDriver(),
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

    generateImages();
}

void Carrot::GBuffer::generateImages() {
    albedoImages.resize(renderer.getSwapchainImageCount());
    albedoImageViews.resize(renderer.getSwapchainImageCount());

    depthImages.resize(renderer.getSwapchainImageCount());
    depthStencilImageViews.resize(renderer.getSwapchainImageCount());
    depthOnlyImageViews.resize(renderer.getSwapchainImageCount());

    viewPositionImages.resize(renderer.getSwapchainImageCount());
    viewPositionImageViews.resize(renderer.getSwapchainImageCount());

    viewNormalImages.resize(renderer.getSwapchainImageCount());
    viewNormalImageViews.resize(renderer.getSwapchainImageCount());

    intPropertiesImages.resize(renderer.getSwapchainImageCount());
    intPropertiesImageViews.resize(renderer.getSwapchainImageCount());

    for(size_t index = 0; index < renderer.getSwapchainImageCount(); index++) {
        // Albedo
        auto swapchainExtent = renderer.getVulkanDriver().getSwapchainExtent();
        albedoImages[index] = move(make_unique<Image>(renderer.getVulkanDriver(),
                                                      vk::Extent3D{swapchainExtent.width, swapchainExtent.height, 1},
                                                      vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment,
                                                      vk::Format::eR8G8B8A8Unorm));

        auto view = albedoImages[index]->createImageView();
        albedoImageViews[index] = std::move(view);

        // Depth
        depthImages[index] = move(make_unique<Image>(renderer.getVulkanDriver(),
                                                     vk::Extent3D{swapchainExtent.width, swapchainExtent.height, 1},
                                                     vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eInputAttachment,
                                                     renderer.getVulkanDriver().getDepthFormat()));

        depthStencilImageViews[index] = move(renderer.getVulkanDriver().createImageView(depthImages[index]->getVulkanImage(), renderer.getVulkanDriver().getDepthFormat(), vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil));
        depthOnlyImageViews[index] = move(renderer.getVulkanDriver().createImageView(depthImages[index]->getVulkanImage(), renderer.getVulkanDriver().getDepthFormat(), vk::ImageAspectFlagBits::eDepth));

        // View-space positions
        viewPositionImages[index] = move(make_unique<Image>(renderer.getVulkanDriver(),
                                                            vk::Extent3D{swapchainExtent.width, swapchainExtent.height, 1},
                                                            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment,
                                                            vk::Format::eR32G32B32A32Sfloat));

        viewPositionImageViews[index] = std::move(viewPositionImages[index]->createImageView(vk::Format::eR32G32B32A32Sfloat));

        // View-space normals
        viewNormalImages[index] = move(make_unique<Image>(renderer.getVulkanDriver(),
                                                          vk::Extent3D{swapchainExtent.width, swapchainExtent.height, 1},
                                                          vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment,
                                                          vk::Format::eR32G32B32A32Sfloat));

        viewNormalImageViews[index] = std::move(viewNormalImages[index]->createImageView(vk::Format::eR32G32B32A32Sfloat));

        // Bitfield for per-pixel properties
        intPropertiesImages[index] = move(make_unique<Image>(renderer.getVulkanDriver(),
                                                          vk::Extent3D{swapchainExtent.width, swapchainExtent.height, 1},
                                                          vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment,
                                                          vk::Format::eR32Uint));

        intPropertiesImageViews[index] = std::move(intPropertiesImages[index]->createImageView(vk::Format::eR32Uint));
    }
}

void Carrot::GBuffer::loadResolvePipeline() {
    gResolvePipeline = renderer.getOrCreatePipeline("gResolve");

    const size_t imageCount = 8/*albedo+depth+viewPosition+normal+lighting+ui+intProperties*/;
    vector<vk::WriteDescriptorSet> writes{renderer.getSwapchainImageCount() * imageCount};
    vector<vk::DescriptorImageInfo> imageInfo{renderer.getSwapchainImageCount() * imageCount};
    for(size_t i = 0; i < renderer.getSwapchainImageCount(); i++) {
        // albedo
        auto& albedoInfo = imageInfo[i*imageCount];
        albedoInfo.imageView = *albedoImageViews[i];
        albedoInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        albedoInfo.sampler = renderer.getVulkanDriver().getLinearSampler();

        auto& writeAlbedo = writes[i*imageCount];
        writeAlbedo.descriptorCount = 1;
        writeAlbedo.descriptorType = vk::DescriptorType::eInputAttachment;
        writeAlbedo.pImageInfo = &albedoInfo;
        writeAlbedo.dstSet = gResolvePipeline->getDescriptorSets0()[i];
        writeAlbedo.dstArrayElement = 0;
        writeAlbedo.dstBinding = 0;

        // depth
        auto& depthInfo = imageInfo[i*imageCount+1];
        depthInfo.imageView = *depthOnlyImageViews[i];
        depthInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        depthInfo.sampler = renderer.getVulkanDriver().getLinearSampler();

        auto& writeDepth = writes[i*imageCount+1];
        writeDepth.descriptorCount = 1;
        writeDepth.descriptorType = vk::DescriptorType::eInputAttachment;
        writeDepth.pImageInfo = &depthInfo;
        writeDepth.dstSet = gResolvePipeline->getDescriptorSets0()[i];
        writeDepth.dstArrayElement = 0;
        writeDepth.dstBinding = 1;

        // view-space positions
        auto& viewPositionInfo = imageInfo[i*imageCount+2];
        viewPositionInfo.imageView = *viewPositionImageViews[i];
        viewPositionInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        viewPositionInfo.sampler = renderer.getVulkanDriver().getLinearSampler();

        auto& writeViewPosition = writes[i*imageCount+2];
        writeViewPosition.descriptorCount = 1;
        writeViewPosition.descriptorType = vk::DescriptorType::eInputAttachment;
        writeViewPosition.pImageInfo = &viewPositionInfo;
        writeViewPosition.dstSet = gResolvePipeline->getDescriptorSets0()[i];
        writeViewPosition.dstArrayElement = 0;
        writeViewPosition.dstBinding = 2;

        // normals
        auto& viewNormalInfo = imageInfo[i*imageCount+3];
        viewNormalInfo.imageView = *viewNormalImageViews[i];
        viewNormalInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        viewNormalInfo.sampler = renderer.getVulkanDriver().getLinearSampler();

        auto& writeViewNormal = writes[i*imageCount+3];
        writeViewNormal.descriptorCount = 1;
        writeViewNormal.descriptorType = vk::DescriptorType::eInputAttachment;
        writeViewNormal.pImageInfo = &viewNormalInfo;
        writeViewNormal.dstSet = gResolvePipeline->getDescriptorSets0()[i];
        writeViewNormal.dstArrayElement = 0;
        writeViewNormal.dstBinding = 3;

        // bitfield
        auto& intPropertiesInfo = imageInfo[i*imageCount+4];
        intPropertiesInfo.imageView = *intPropertiesImageViews[i];
        intPropertiesInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        intPropertiesInfo.sampler = renderer.getVulkanDriver().getNearestSampler();

        auto& writeIntProperties = writes[i*imageCount+4];
        writeIntProperties.descriptorCount = 1;
        writeIntProperties.descriptorType = vk::DescriptorType::eInputAttachment;
        writeIntProperties.pImageInfo = &intPropertiesInfo;
        writeIntProperties.dstSet = gResolvePipeline->getDescriptorSets0()[i];
        writeIntProperties.dstArrayElement = 0;
        writeIntProperties.dstBinding = 4;

        // raytraced lighting
        auto& lightingNfo = imageInfo[i*imageCount+5];
        lightingNfo.imageView = *raytracer.getLightingImageViews()[i];
        lightingNfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        lightingNfo.sampler = nullptr;

        auto& writeLighting = writes[i*imageCount+5];
        writeLighting.descriptorCount = 1;
        writeLighting.descriptorType = vk::DescriptorType::eSampledImage;
        writeLighting.pImageInfo = &lightingNfo;
        writeLighting.dstSet = gResolvePipeline->getDescriptorSets0()[i];
        writeLighting.dstArrayElement = 0;
        writeLighting.dstBinding = 5;

        // ui
        auto& uiInfo = imageInfo[i*imageCount+6];
        uiInfo.imageView = *renderer.getUIImageViews()[i];
        uiInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        uiInfo.sampler = nullptr;

        auto& writeUI = writes[i*imageCount+6];
        writeUI.descriptorCount = 1;
        writeUI.descriptorType = vk::DescriptorType::eSampledImage;
        writeUI.pImageInfo = &uiInfo;
        writeUI.dstSet = gResolvePipeline->getDescriptorSets0()[i];
        writeUI.dstArrayElement = 0;
        writeUI.dstBinding = 6;

        // skybox
        auto& skyboxInfo = imageInfo[i*imageCount+7];
        skyboxInfo.imageView = *renderer.getSkyboxImageViews()[i];
        skyboxInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        skyboxInfo.sampler = nullptr;

        auto& writeSkybox = writes[i*imageCount+7];
        writeSkybox.descriptorCount = 1;
        writeSkybox.descriptorType = vk::DescriptorType::eSampledImage;
        writeSkybox.pImageInfo = &skyboxInfo;
        writeSkybox.dstSet = gResolvePipeline->getDescriptorSets0()[i];
        writeSkybox.dstArrayElement = 0;
        writeSkybox.dstBinding = 10;
    }
    renderer.getVulkanDriver().getLogicalDevice().updateDescriptorSets(writes, {});
}

void Carrot::GBuffer::recordResolvePass(uint32_t frameIndex, vk::CommandBuffer& commandBuffer, vk::CommandBufferInheritanceInfo* inheritance) const {
    vk::CommandBufferBeginInfo beginInfo{
            .flags = vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse,
            .pInheritanceInfo = inheritance,
    };

    commandBuffer.begin(beginInfo);
    {
        renderer.getVulkanDriver().updateViewportAndScissor(commandBuffer);
        gResolvePipeline->bind(frameIndex, commandBuffer);
        screenQuadMesh->bind(commandBuffer);
        screenQuadMesh->draw(commandBuffer);
    }
    commandBuffer.end();
}

void Carrot::GBuffer::addFramebufferAttachments(uint32_t frameIndex, vector<vk::ImageView>& attachments) {
    attachments.push_back(*depthOnlyImageViews[frameIndex]);
    attachments.push_back(*albedoImageViews[frameIndex]);
    attachments.push_back(*viewPositionImageViews[frameIndex]);
    attachments.push_back(*viewNormalImageViews[frameIndex]);
    attachments.push_back(*intPropertiesImageViews[frameIndex]);
}

vk::UniqueRenderPass Carrot::GBuffer::createRenderPass() {
    vk::AttachmentDescription finalColorAttachment {
            .format = renderer.getVulkanDriver().getSwapchainImageFormat(),
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
            .format = renderer.getVulkanDriver().getDepthFormat(),
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

    vk::AttachmentDescription gBufferIntPropertiesAttachment {
            .format = vk::Format::eR32Uint,
            .samples = vk::SampleCountFlagBits::e1,

            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,

            .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
            .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,

            .initialLayout = vk::ImageLayout::eUndefined,
            .finalLayout = vk::ImageLayout::eColorAttachmentOptimal,
    };

    vk::AttachmentReference gBufferIntPropertiesOutputAttachmentRef{
            .attachment = 5,
            .layout = vk::ImageLayout::eColorAttachmentOptimal,
    };

    vk::AttachmentReference gBufferIntPropertiesInputAttachmentRef{
            .attachment = 5,
            .layout = vk::ImageLayout::eShaderReadOnlyOptimal,
    };

    vk::AttachmentReference outputs[] = {
            gBufferColorOutputAttachmentRef,
            gBufferViewPositionOutputAttachmentRef,
            gBufferNormalOutputAttachmentRef,
            gBufferIntPropertiesOutputAttachmentRef,
    };

    vk::SubpassDescription gBufferSubpass {
            .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
            .inputAttachmentCount = 0,

            .colorAttachmentCount = sizeof(outputs) / sizeof(vk::AttachmentReference),
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
            gBufferIntPropertiesInputAttachmentRef,
    };

    vk::SubpassDescription gResolveSubpass {
            .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
            .inputAttachmentCount = sizeof(resolveInputs)/sizeof(vk::AttachmentReference),
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
                    .dstSubpass = RenderPasses::GResolveSubPassIndex,

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
            gBufferIntPropertiesAttachment,
    };

    vk::RenderPassCreateInfo renderPassInfo{
            .attachmentCount = static_cast<uint32_t>(attachments.size()),
            .pAttachments = attachments.data(),
            .subpassCount = sizeof(subpasses)/sizeof(vk::SubpassDescription),
            .pSubpasses = subpasses,

            .dependencyCount = sizeof(dependencies)/sizeof(vk::SubpassDependency),
            .pDependencies = dependencies,
    };

    return renderer.getVulkanDriver().getLogicalDevice().createRenderPassUnique(renderPassInfo, renderer.getVulkanDriver().getAllocationCallbacks());
}

void Carrot::GBuffer::onSwapchainImageCountChange(size_t newCount) {

}

void Carrot::GBuffer::onSwapchainSizeChange(int newWidth, int newHeight) {
    generateImages();
    loadResolvePipeline();
}
