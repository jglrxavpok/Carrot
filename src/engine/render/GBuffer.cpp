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
    albedoTextures.resize(renderer.getSwapchainImageCount());
    depthStencilTextures.resize(renderer.getSwapchainImageCount());
    viewPositionTextures.resize(renderer.getSwapchainImageCount());
    viewNormalTextures.resize(renderer.getSwapchainImageCount());
    intPropertiesTextures.resize(renderer.getSwapchainImageCount());

    for(size_t index = 0; index < renderer.getSwapchainImageCount(); index++) {
        // Albedo
        auto swapchainExtent = renderer.getVulkanDriver().getSwapchainExtent();
        albedoTextures[index] = move(make_unique<Render::Texture>(renderer.getVulkanDriver(),
                                                      vk::Extent3D{swapchainExtent.width, swapchainExtent.height, 1},
                                                      vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment | vk::ImageUsageFlagBits::eSampled,
                                                      vk::Format::eR8G8B8A8Unorm));


        // Depth
        depthStencilTextures[index] = move(make_unique<Render::Texture>(renderer.getVulkanDriver(),
                                                     vk::Extent3D{swapchainExtent.width, swapchainExtent.height, 1},
                                                     vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eInputAttachment | vk::ImageUsageFlagBits::eSampled,
                                                     renderer.getVulkanDriver().getDepthFormat()));

        // View-space positions
        viewPositionTextures[index] = move(make_unique<Render::Texture>(renderer.getVulkanDriver(),
                                                            vk::Extent3D{swapchainExtent.width, swapchainExtent.height, 1},
                                                            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment | vk::ImageUsageFlagBits::eSampled,
                                                            vk::Format::eR32G32B32A32Sfloat));

        // View-space normals
        viewNormalTextures[index] = move(make_unique<Render::Texture>(renderer.getVulkanDriver(),
                                                          vk::Extent3D{swapchainExtent.width, swapchainExtent.height, 1},
                                                          vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment | vk::ImageUsageFlagBits::eSampled,
                                                          vk::Format::eR32G32B32A32Sfloat));

        // Bitfield for per-pixel properties
        intPropertiesTextures[index] = move(make_unique<Render::Texture>(renderer.getVulkanDriver(),
                                                          vk::Extent3D{swapchainExtent.width, swapchainExtent.height, 1},
                                                          vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment | vk::ImageUsageFlagBits::eSampled,
                                                          vk::Format::eR32Uint));

        albedoTextures[index]->assumeLayout(vk::ImageLayout::eColorAttachmentOptimal);
        depthStencilTextures[index]->assumeLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
        viewPositionTextures[index]->assumeLayout(vk::ImageLayout::eColorAttachmentOptimal);
        viewNormalTextures[index]->assumeLayout(vk::ImageLayout::eColorAttachmentOptimal);
        intPropertiesTextures[index]->assumeLayout(vk::ImageLayout::eColorAttachmentOptimal);
    }
}

void Carrot::GBuffer::loadResolvePipeline() {
    gResolvePipeline = renderer.getOrCreatePipeline(renderer.getGRenderPass(), "gResolve");

    const size_t imageCount = 8/*albedo+depth+viewPosition+normal+lighting+ui+intProperties*/;
    vector<vk::WriteDescriptorSet> writes{renderer.getSwapchainImageCount() * imageCount};
    vector<vk::DescriptorImageInfo> imageInfo{renderer.getSwapchainImageCount() * imageCount};
    for(size_t i = 0; i < renderer.getSwapchainImageCount(); i++) {
        // albedo
        auto& albedoInfo = imageInfo[i*imageCount];
        albedoInfo.imageView = albedoTextures[i]->getView();
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
        depthInfo.imageView = depthStencilTextures[i]->getView(vk::ImageAspectFlagBits::eDepth);
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
        viewPositionInfo.imageView = viewPositionTextures[i]->getView();
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
        viewNormalInfo.imageView = viewNormalTextures[i]->getView();
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
        intPropertiesInfo.imageView = intPropertiesTextures[i]->getView();
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
        lightingNfo.imageView = raytracer.getLightingTextures()[i]->getView();
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
        uiInfo.imageView = renderer.getUITextures()[i]->getView();
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
        skyboxInfo.imageView = renderer.getSkyboxTextures()[i]->getView();
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
        gResolvePipeline->bind(renderer.getGRenderPass(), frameIndex, commandBuffer);
        screenQuadMesh->bind(commandBuffer);
        screenQuadMesh->draw(commandBuffer);
    }
    commandBuffer.end();
}

void Carrot::GBuffer::addFramebufferAttachments(uint32_t frameIndex, vector<vk::ImageView>& attachments) {
    attachments.push_back(depthStencilTextures[frameIndex]->getView(vk::ImageAspectFlagBits::eDepth));
    attachments.push_back(albedoTextures[frameIndex]->getView());
    attachments.push_back(viewPositionTextures[frameIndex]->getView());
    attachments.push_back(viewNormalTextures[frameIndex]->getView());
    attachments.push_back(intPropertiesTextures[frameIndex]->getView());
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

Carrot::Render::Pass<Carrot::GBuffer::GBufferData>& Carrot::GBuffer::addGBufferPass(Carrot::Render::GraphBuilder& graph, std::function<void(const Carrot::Render::CompiledPass& pass, const Carrot::Render::FrameData&, vk::CommandBuffer&)> callback) {
    using namespace Carrot::Render;
    vk::ClearValue clearColor = vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,1.0f});
    vk::ClearValue positionClear = vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,0.0f});
    vk::ClearValue clearDepth = vk::ClearDepthStencilValue{
            .depth = 1.0f,
            .stencil = 0
    };
    vk::ClearValue clearIntProperties = vk::ClearColorValue();
    auto& swapchainExtent = renderer.getVulkanDriver().getSwapchainExtent();
    return graph.addPass<Carrot::GBuffer::GBufferData>("gbuffer",
           [&](GraphBuilder& graph, Pass<GBufferData>& pass, GBufferData& data)
           {

                data.albedo = graph.createRenderTarget(vk::Format::eR8G8B8A8Unorm,
                                                       vk::Extent3D{swapchainExtent.width, swapchainExtent.height, 1},
                                                       vk::AttachmentLoadOp::eClear,
                                                       clearColor,
                                                       vk::ImageLayout::eColorAttachmentOptimal);

                data.depthStencil = graph.createRenderTarget(renderer.getVulkanDriver().getDepthFormat(),
                                                             vk::Extent3D{swapchainExtent.width, swapchainExtent.height, 1},
                                                             vk::AttachmentLoadOp::eClear,
                                                             clearDepth,
                                                             vk::ImageLayout::eDepthStencilAttachmentOptimal);

                data.positions = graph.createRenderTarget(vk::Format::eR32G32B32A32Sfloat,
                                                          vk::Extent3D{swapchainExtent.width, swapchainExtent.height, 1},
                                                          vk::AttachmentLoadOp::eClear,
                                                          positionClear,
                                                          vk::ImageLayout::eColorAttachmentOptimal);

                data.normals = graph.createRenderTarget(vk::Format::eR32G32B32A32Sfloat,
                                                        vk::Extent3D{swapchainExtent.width, swapchainExtent.height, 1},
                                                        vk::AttachmentLoadOp::eClear,
                                                        positionClear,
                                                        vk::ImageLayout::eColorAttachmentOptimal);

                data.flags = graph.createRenderTarget(vk::Format::eR32Uint,
                                                      vk::Extent3D{swapchainExtent.width, swapchainExtent.height, 1},
                                                      vk::AttachmentLoadOp::eClear,
                                                      clearIntProperties,
                                                      vk::ImageLayout::eColorAttachmentOptimal);
           },
           [callback](const Render::CompiledPass& pass, const Render::FrameData& frame, const GBufferData& data, vk::CommandBuffer& buffer){
                callback(pass, frame, buffer);
           }
    );
}

Carrot::Render::Pass<Carrot::GBuffer::GResolveData>& Carrot::GBuffer::addGResolvePass(const Carrot::GBuffer::GBufferData& data,
                                                                                      const Carrot::GBuffer::RaytracingPassData& rtData,
                                                                                      const Carrot::ImGuiPassData& imguiData,
                                                                                      const Carrot::Render::FrameResource& skyboxOutput,
                                                                                      Carrot::Render::GraphBuilder& graph) {
    using namespace Carrot::Render;
    vk::ClearValue clearColor = vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,1.0f});
    auto& swapchainExtent = renderer.getVulkanDriver().getSwapchainExtent();
    return graph.addPass<Carrot::GBuffer::GResolveData>("gresolve",
           [&, skyboxOutput = skyboxOutput](GraphBuilder& graph, Pass<GResolveData>& pass, GResolveData& resolveData)
           {
                resolveData.positions = graph.read(data.positions, vk::ImageLayout::eShaderReadOnlyOptimal);
                resolveData.normals = graph.read(data.normals, vk::ImageLayout::eShaderReadOnlyOptimal);
                resolveData.albedo = graph.read(data.albedo, vk::ImageLayout::eShaderReadOnlyOptimal);
                resolveData.depthStencil = graph.read(data.depthStencil, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil);
                resolveData.flags = graph.read(data.flags, vk::ImageLayout::eShaderReadOnlyOptimal);
                resolveData.raytracing = graph.read(rtData.output, vk::ImageLayout::eShaderReadOnlyOptimal);
                resolveData.ui = graph.read(imguiData.output, vk::ImageLayout::eShaderReadOnlyOptimal);
                resolveData.skybox = graph.read(skyboxOutput, vk::ImageLayout::eShaderReadOnlyOptimal);
                resolveData.resolved = graph.createRenderTarget(vk::Format::eR8G8B8A8Unorm,
                                                        vk::Extent3D{swapchainExtent.width, swapchainExtent.height, 1},
                                                        vk::AttachmentLoadOp::eClear,
                                                        clearColor,
                                                        vk::ImageLayout::eColorAttachmentOptimal);
           },
           [this](const Render::CompiledPass& pass, const Render::FrameData& frame, const GResolveData& data, vk::CommandBuffer& buffer) {
                auto resolvePipeline = renderer.getOrCreatePipeline(pass.getRenderPass(), "gResolve-rendergraph");
                renderer.bindTexture(*resolvePipeline, frame, pass.getGraph().getTexture(data.albedo, frame.frameIndex), 0, 0, nullptr);
                renderer.bindTexture(*resolvePipeline, frame, pass.getGraph().getTexture(data.depthStencil, frame.frameIndex), 0, 1, nullptr, vk::ImageAspectFlagBits::eDepth);
                renderer.bindTexture(*resolvePipeline, frame, pass.getGraph().getTexture(data.positions, frame.frameIndex), 0, 2, nullptr);
                renderer.bindTexture(*resolvePipeline, frame, pass.getGraph().getTexture(data.normals, frame.frameIndex), 0, 3, nullptr);
                renderer.bindTexture(*resolvePipeline, frame, pass.getGraph().getTexture(data.flags, frame.frameIndex), 0, 4, renderer.getVulkanDriver().getNearestSampler());
                renderer.bindTexture(*resolvePipeline, frame, pass.getGraph().getTexture(data.raytracing, frame.frameIndex), 0, 5, nullptr);
                renderer.bindTexture(*resolvePipeline, frame, pass.getGraph().getTexture(data.ui, frame.frameIndex), 0, 6, nullptr);

                renderer.bindTexture(*resolvePipeline, frame, pass.getGraph().getTexture(data.skybox, frame.frameIndex), 0, 10, nullptr);

                resolvePipeline->bind(pass.getRenderPass(), frame.frameIndex, buffer);
                screenQuadMesh->bind(buffer);
                screenQuadMesh->draw(buffer);
           }
    );
}
