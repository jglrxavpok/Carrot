//
// Created by jglrxavpok on 13/07/2021.
//

#ifdef ENABLE_VR
#include "Session.h"
#include "VRInterface.h"
#include "engine/utils/Macros.h"
#include "engine/render/VulkanRenderer.h"
#include "engine/render/resources/Mesh.h"
#include "engine/render/resources/Pipeline.h"
#include "engine/render/TextureRepository.h"
#include "engine/render/Camera.h"
#include "engine/utils/conversions.h"

namespace Carrot::VR {
    Session::Session(Interface& vr): vr(vr) {
        xr::SessionCreateInfo createInfo;

        xr::GraphicsRequirementsVulkanKHR vulkanGraphics;
        vr.getXRInstance().getVulkanGraphicsRequirements2KHR(vr.getXRSystemID(), vulkanGraphics, vr.getXRDispatch());
        auto& config = vr.getEngine().getConfiguration();

        auto vkMajor = VK_VERSION_MAJOR(config.vulkanApiVersion);
        auto vkMinor = VK_VERSION_MINOR(config.vulkanApiVersion);
        auto vkPatch = VK_VERSION_PATCH(config.vulkanApiVersion);
        auto vkVersion = XR_MAKE_VERSION(vkMajor, vkMinor, vkPatch);
        bool compatible = vulkanGraphics.minApiVersionSupported <= vkVersion && vulkanGraphics.maxApiVersionSupported >= vkVersion;
        if(!compatible) {
            throw std::runtime_error("Incompatible Vulkan versions.");
        }

        // TODO: check versions
        // TODO: xrGetVulkanGraphicsDeviceKHR
        // TODO: xrGetVulkanDeviceExtensionsKHR
        auto systemProperties = vr.getXRInstance().getSystemProperties(vr.getXRSystemID());
        auto& systemGraphicalProperties = systemProperties.graphicsProperties;

        xr::GraphicsBindingVulkanKHR vulkanBindings;
        vulkanBindings.device = vr.getEngine().getVulkanDriver().getLogicalDevice();
        vulkanBindings.instance = vr.getEngine().getVulkanDriver().getInstance();
        vulkanBindings.physicalDevice = vr.getEngine().getVulkanDriver().getPhysicalDevice();
        vulkanBindings.queueFamilyIndex = vr.getEngine().getVulkanDriver().getQueueFamilies().graphicsFamily.value();
        vulkanBindings.queueIndex = vr.getEngine().getVulkanDriver().getGraphicsQueueIndex();

        createInfo.next = &vulkanBindings;
        createInfo.createFlags = xr::SessionCreateFlagBits::None;
        createInfo.systemId = vr.getXRSystemID();

        xrSession = vr.getXRInstance().createSessionUnique(createInfo);
        vr.registerSession(this);

        xr::ReferenceSpaceCreateInfo spaceReference;
        // TODO: seated play will use "View"
        spaceReference.referenceSpaceType = xr::ReferenceSpaceType::Local;

        xrSpace = xrSession->createReferenceSpaceUnique(spaceReference);

        auto swapchainFormats = xrSession->enumerateSwapchainFormatsToVector();
        bool foundCompatible = false;

        for(const auto& format : swapchainFormats) {
            if(static_cast<std::int64_t>(vk::Format::eR8G8B8A8Unorm) == format
            || static_cast<std::int64_t>(vk::Format::eR8G8B8A8Srgb) == format
            ) {
                swapchainFormat = static_cast<vk::Format>(format);
                foundCompatible = true;
                break;
            }
        }
        assert(foundCompatible);

        // TODO: Linear format may not be best https://developer.nvidia.com/gpugems/gpugems3/part-iv-image-effects/chapter-24-importance-being-linear


        auto viewConfigs = vr.getXRInstance().enumerateViewConfigurationViewsToVector(vr.getXRSystemID(), xr::ViewConfigurationType::PrimaryStereo);
        assert(viewConfigs.size() == 2);
        assert(viewConfigs[0].recommendedImageRectHeight == viewConfigs[1].recommendedImageRectHeight);

        fullSwapchainSize = vk::Extent2D { viewConfigs[0].maxImageRectWidth * 2, viewConfigs[1].maxImageRectHeight };
        eyeRenderSize = vk::Extent2D { viewConfigs[0].maxImageRectWidth, viewConfigs[1].maxImageRectHeight };

        xr::SwapchainCreateInfo swapchainInfo;
        swapchainInfo.width = fullSwapchainSize.width;
        swapchainInfo.height = fullSwapchainSize.height;
        swapchainInfo.mipCount = 1;
        swapchainInfo.sampleCount = 1;
        swapchainInfo.arraySize = 2;
        swapchainInfo.faceCount = 1;
        swapchainInfo.format = static_cast<int64_t>(swapchainFormat);
        swapchainInfo.createFlags = xr::SwapchainCreateFlagBits::ProtectedContent;
        swapchainInfo.usageFlags = xr::SwapchainUsageFlagBits::TransferDst;
        xrSwapchain = xrSession->createSwapchainUnique(swapchainInfo);

        xrSwapchainImages = xrSwapchain->enumerateSwapchainImagesToVector<xr::SwapchainImageVulkanKHR>();

        xrSwapchainTextures.resize(xrSwapchainImages.size());
        for (int i = 0; i < xrSwapchainImages.size(); i++) {
            xrSwapchainTextures[i] = std::make_shared<Carrot::Render::Texture>(
                    vr.getEngine().getVulkanDriver(),
                    xrSwapchainImages[i].image,
                    vk::Extent3D { fullSwapchainSize.width, fullSwapchainSize.height, 1 },
                    swapchainFormat
            );
        }

        auto& driver = vr.getEngine().getVulkanDriver();
        blitCommandPool = driver.getLogicalDevice().createCommandPoolUnique(vk::CommandPoolCreateInfo {
                .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                .queueFamilyIndex = driver.getQueueFamilies().graphicsFamily.value(),
        });

        renderFences.resize(xrSwapchainTextures.size());

        for(int i = 0; i < xrSwapchainTextures.size(); i++) {
            renderFences[i] = std::move(driver.getLogicalDevice().createFenceUnique(vk::FenceCreateInfo {
                .flags = vk::FenceCreateFlagBits::eSignaled,
            }));
        }
    }

    void Session::stateChanged(xr::Time time, xr::SessionState state) {
        if(state == xr::SessionState::Ready) {
            readyForRendering = true;
            xr::SessionBeginInfo beginInfo;
            beginInfo.primaryViewConfigurationType = xr::ViewConfigurationType::PrimaryStereo;
            xrSession->beginSession(beginInfo);
        }
    }

    void Session::xrWaitFrame() {
        ZoneScopedN("VR Wait frame");
        xr::FrameState state = xrSession->waitFrame({});
        shouldRender = (bool)state.shouldRender;
        predictedEndTime = state.predictedDisplayTime;

        xr::ViewState viewState;
        xr::ViewLocateInfo locateInfo;
        locateInfo.viewConfigurationType = xr::ViewConfigurationType::PrimaryStereo;
        locateInfo.displayTime = predictedEndTime;
        locateInfo.space = *xrSpace;
        {
            ZoneScopedN("Get views");
            xrViews = xrSession->locateViewsToVector(locateInfo, reinterpret_cast<XrViewState*>(&viewState));
        }

        auto updateCamera = [&](Carrot::Render::Eye eye, xr::View& view) {
            glm::vec3 translation{0.0f};
            auto& camera = getEngine().getCamera(eye);
            auto tmp = Carrot::toGlm(view.pose);
            // used to align with engine orientation
            glm::mat4 correctOrientation = glm::rotate(-glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f));
            camera.getViewMatrixRef() = glm::inverse(tmp) * correctOrientation;// * glm::translate({}, translation);
            camera.getProjectionMatrixRef() = Carrot::toGlm(view.fov);
        };

        updateCamera(Carrot::Render::Eye::LeftEye, xrViews[0]);
        updateCamera(Carrot::Render::Eye::RightEye, xrViews[1]);
    }

    xr::Result Session::xrBeginFrame() {
        ZoneScopedN("xrSession->beginFrame({})");
        return xrSession->beginFrame({});
    }

    void Session::xrEndFrame() {
        ZoneScopedN("VR End Frame");
        xrSwapchain->releaseSwapchainImage({});

        auto updateView = [&](std::uint32_t index) {
            auto& view = xrProjectionViews[index];
            view.pose = xrViews[index].pose;
            view.fov = xrViews[index].fov;

            xr::SwapchainSubImage& subImage = view.subImage;
            subImage.imageArrayIndex = 0;
            subImage.imageRect = xr::Rect2Di(xr::Offset2Di(eyeRenderSize.width * index, 0), xr::Extent2Di(eyeRenderSize.width, eyeRenderSize.height));
            subImage.swapchain = *xrSwapchain;
        };

        updateView(0); // left eye
        updateView(1); // right eye

        xr::CompositionLayerProjection eyes;
/*        eyes.layerFlags = xr::CompositionLayerFlagBits::UnpremultipliedAlpha;
        eyes.layerFlags = xr::CompositionLayerFlagBits::CorrectChromaticAberration;*/
        eyes.space = *xrSpace;
        eyes.viewCount = 2;
        eyes.views = xrProjectionViews;

        const xr::CompositionLayerBaseHeader * const layers [] = { &eyes };

        xr::FrameEndInfo frameEndInfo;
        frameEndInfo.displayTime = predictedEndTime;
        frameEndInfo.environmentBlendMode = xr::EnvironmentBlendMode::Opaque;

        frameEndInfo.layerCount = 1;
        frameEndInfo.layers = layers;

        xrSession->endFrame(frameEndInfo);
    }

    void Session::setEyeTexturesToPresent(const Render::FrameResource& leftEye, const Render::FrameResource& rightEye) {
        this->leftEye = leftEye;
        this->rightEye = rightEye;

        auto& repo = vr.getEngine().getVulkanDriver().getTextureRepository();
        repo.getUsages(leftEye.rootID) |= vk::ImageUsageFlagBits::eTransferSrc;
        repo.getUsages(rightEye.rootID) |= vk::ImageUsageFlagBits::eTransferSrc;
    }

    void Session::startFrame() {
        if(!isReadyForRendering())
            return;
        xrWaitFrame();
    }

    void Session::present(const Render::Context& regularRenderContext) {
        if(!isReadyForRendering())
            return;
        auto result = xrBeginFrame();
        if(result == xr::Result::FrameDiscarded) {
            return;
        }

        {
            ZoneScopedN("VR Acquire swapchain");
            xrSwapchainIndex = xrSwapchain->acquireSwapchainImage({});
        }
        {
            ZoneScopedN("VR Wait swapchain");
            xr::SwapchainImageWaitInfo waitInfo;
            waitInfo.timeout = xr::Duration::infinite();
            xrSwapchain->waitSwapchainImage(waitInfo);
        }

        if(shouldRenderToSwapchain()) {
            auto& fence = *renderFences[xrSwapchainIndex];
            auto& device = getEngine().getVulkanDriver().getLogicalDevice();
            DISCARD(device.waitForFences(fence, true, UINT64_MAX));
            device.resetFences(fence);

            auto& driver = vr.getEngine().getVulkanDriver();

            if(blitCommandBuffers.empty()) {
                ZoneScopedN("VR Blit to swapchain");
                // records command buffers

                blitCommandBuffers = driver.getLogicalDevice().allocateCommandBuffers(vk::CommandBufferAllocateInfo {
                        .commandPool = *blitCommandPool,
                        .level = vk::CommandBufferLevel::ePrimary,
                        .commandBufferCount = static_cast<uint32_t>(xrSwapchainTextures.size()),
                });

                auto blit = [&](vk::CommandBuffer& cmds, const Render::FrameResource& eyeTexture, std::uint32_t index, std::uint32_t swapchainIndex) {
                    auto& texture = vr.getEngine().getVulkanDriver().getTextureRepository().get(eyeTexture, swapchainIndex);
                    vk::ImageBlit blitInfo;
                    blitInfo.dstSubresource.aspectMask = blitInfo.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
                    blitInfo.dstSubresource.layerCount = blitInfo.srcSubresource.layerCount = 1;
                    blitInfo.dstOffsets[0] = vk::Offset3D{ (int32_t)(eyeRenderSize.width * index), 0, 0 };
                    blitInfo.dstOffsets[1] = vk::Offset3D{ (int32_t)(eyeRenderSize.width * (index+1)), (int32_t)eyeRenderSize.height, 1 };
                    blitInfo.srcOffsets[1] = vk::Offset3D{ static_cast<int32_t>(texture.getSize().width), static_cast<int32_t>(texture.getSize().height), 1 };

                    auto& xrSwapchainTexture = *xrSwapchainTextures[swapchainIndex];
                    xrSwapchainTexture.assumeLayout(vk::ImageLayout::eUndefined);
                    xrSwapchainTexture.transitionInline(cmds, vk::ImageLayout::eTransferDstOptimal);

                    texture.assumeLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
                    texture.transitionInline(cmds, vk::ImageLayout::eTransferSrcOptimal);
                    cmds.blitImage(texture.getVulkanImage(), vk::ImageLayout::eTransferSrcOptimal, xrSwapchainTexture.getVulkanImage(), vk::ImageLayout::eTransferDstOptimal, blitInfo, vk::Filter::eNearest);
                    xrSwapchainTexture.transitionInline(cmds, vk::ImageLayout::eColorAttachmentOptimal); // OpenXR & Vulkan complain if this is not done? eTransferSrcOptimal makes more sense to me
                    //xrSwapchainTexture.transitionInline(cmds, vk::ImageLayout::eTransferSrcOptimal);
                    texture.transitionInline(cmds, vk::ImageLayout::eColorAttachmentOptimal);
                };
                for(int i = 0; i < xrSwapchainTextures.size(); i++) {
                    auto& cmds = blitCommandBuffers[i];
                    DISCARD(cmds.begin(vk::CommandBufferBeginInfo {
                    }));
                    blit(cmds, leftEye, 0, i);
                    blit(cmds, rightEye, 1, i);
                    cmds.end();
                }
            }


            ZoneScopedN("Submit");
            vr.getEngine().getGraphicsQueue().submit(vk::SubmitInfo {
                    .commandBufferCount = 1,
                    .pCommandBuffers = &blitCommandBuffers[xrSwapchainIndex],
            }, *renderFences[xrSwapchainIndex]);
        }

        xrEndFrame();
    }

    Carrot::Engine& Session::getEngine() {
        return vr.getEngine();
    }

    Session::~Session() {
        vr.unregisterSession(this);
    }
}
#endif