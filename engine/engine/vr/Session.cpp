//
// Created by jglrxavpok on 13/07/2021.
//

#include "Session.h"

#include <engine/Engine.h>

#include "VRInterface.h"
#include "engine/utils/Macros.h"
#include "engine/render/VulkanRenderer.h"
#include "engine/render/resources/Mesh.h"
#include "engine/render/resources/Pipeline.h"
#include "engine/render/TextureRepository.h"
#include "engine/render/Camera.h"
#include "engine/utils/conversions.h"
#include "engine/io/actions/ActionSet.h"

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

        spaceReference.referenceSpaceType = xr::ReferenceSpaceType::View;
        headSpace = xrSession->createReferenceSpaceUnique(spaceReference);

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

        fullSwapchainSize = vk::Extent2D { viewConfigs[0].recommendedImageRectWidth * 2, viewConfigs[1].recommendedImageRectHeight };
        eyeRenderSize = vk::Extent2D { viewConfigs[0].recommendedImageRectWidth, viewConfigs[1].recommendedImageRectHeight };

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

        // --

        handTracking = std::make_unique<HandTracking>(vr, *this);
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

        TracyPlot("Display duration (ms)", state.predictedDisplayPeriod.get()/1000000.0);

        xr::ViewState viewState;
        xr::ViewLocateInfo locateInfo;
        locateInfo.viewConfigurationType = xr::ViewConfigurationType::PrimaryStereo;
        locateInfo.displayTime = predictedEndTime;
        locateInfo.space = *xrSpace;
        {
            ZoneScopedN("Get views");
            xrViews = xrSession->locateViewsToVector(locateInfo, reinterpret_cast<XrViewState*>(&viewState));
        }

        xr::SpaceLocation headLocation = locateSpace(*headSpace);
        headPosition = Carrot::xrSpaceToCarrotSpace(Carrot::toGlm(headLocation.pose.position));
        headRotation = Carrot::xrSpaceToCarrotSpace(Carrot::toGlm(headLocation.pose.orientation));

        auto updateCamera = [&](Carrot::Render::Eye eye, xr::View& view) {
            glm::vec3 translation{0.0f};
            auto& camera = getEngine().getMainViewportCamera(eye);
            auto tmp = Carrot::toGlm(view.pose);
            // used to align with engine orientation
            glm::mat4 correctOrientation = glm::rotate(-glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f));
            eyeViews[static_cast<int>(eye)] = glm::inverse(tmp) * correctOrientation;// * glm::translate({}, translation);
            eyeProjections[static_cast<int>(eye)] = Carrot::toGlm(view.fov);
        };

        updateCamera(Carrot::Render::Eye::LeftEye, xrViews[0]);
        updateCamera(Carrot::Render::Eye::RightEye, xrViews[1]);

        // --

        handTracking->locateJoints(*xrSpace, predictedEndTime);
    }

    xr::Result Session::xrBeginFrame() {
        ZoneScopedN("xrSession->beginFrame({})");
        return xrSession->beginFrame({});
    }

    void Session::xrEndFrame() {
        ZoneScopedN("VR End Frame");
        {
            ZoneScopedN("Release swapchain");
            xrSwapchain->releaseSwapchainImage({});
        }

        auto updateView = [&](std::uint32_t index) {
            ZoneScopedN("Update eye");
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

        {
            ZoneScopedN("xrSession->endFrame(frameEndInfo);");
            xrSession->endFrame(frameEndInfo);
        }
    }

    const glm::mat4& Session::getEyeProjection(Carrot::Render::Eye eye) const {
        switch(eye) {
            case Carrot::Render::Eye::LeftEye:
                return eyeProjections[0];

            case Carrot::Render::Eye::RightEye:
                return eyeProjections[1];

            default:
                verify(false, "invalid case");
                throw;
        }
    }

    const glm::mat4& Session::getEyeView(Carrot::Render::Eye eye) const {
        switch(eye) {
            case Carrot::Render::Eye::LeftEye:
                return eyeViews[0];

            case Carrot::Render::Eye::RightEye:
                return eyeViews[1];

            default:
                verify(false, "invalid case");
                throw;
        }
    }

    const HandTracking& Session::getHandTracking() const {
        return *handTracking;
    }

    const glm::vec3& Session::getHeadPosition() const {
        return headPosition;
    }

    const glm::quat& Session::getHeadRotation() const {
        return headRotation;
    }

    xr::UniqueActionSet Session::createActionSet(std::string_view name) {
        xr::ActionSetCreateInfo createInfo;
        Carrot::strcpy(createInfo.actionSetName, sizeof(createInfo.actionSetName), name.data());
        Carrot::strcpy(createInfo.localizedActionSetName, sizeof(createInfo.localizedActionSetName), name.data());
        return vr.getXRInstance().createActionSetUnique(createInfo);
    }

    xr::Path Session::makeXRPath(const std::string& path) {
        return vr.getXRInstance().stringToPath(path.c_str());
    }

    void Session::suggestBindings(const std::string& interactionProfile, std::span<const xr::ActionSuggestedBinding> bindings) {
        xr::InteractionProfileSuggestedBinding suggestion;
        suggestion.interactionProfile = makeXRPath(interactionProfile);
        suggestion.countSuggestedBindings = bindings.size();
        suggestion.suggestedBindings = bindings.data();
        vr.getXRInstance().suggestInteractionProfileBindings(suggestion);
    }

    xr::ActionStateBoolean Session::getActionStateBoolean(Carrot::IO::BoolInputAction& action) {
        xr::ActionStateGetInfo getInfo;
        getInfo.action = action.getXRAction();
        return xrSession->getActionStateBoolean(getInfo);
    }

    xr::ActionStateFloat Session::getActionStateFloat(Carrot::IO::FloatInputAction& action) {
        xr::ActionStateGetInfo getInfo;
        getInfo.action = action.getXRAction();
        return xrSession->getActionStateFloat(getInfo);
    }

    xr::ActionStateVector2f Session::getActionStateVector2f(Carrot::IO::Vec2InputAction& action) {
        xr::ActionStateGetInfo getInfo;
        getInfo.action = action.getXRAction();
        return xrSession->getActionStateVector2f(getInfo);
    }

    xr::ActionStatePose Session::getActionStatePose(Carrot::IO::PoseInputAction& action) {
        xr::ActionStateGetInfo getInfo;
        getInfo.action = action.getXRAction();
        return xrSession->getActionStatePose(getInfo);
    }

    xr::SpaceLocation Session::locateSpace(xr::Space& space) {
        return space.locateSpace(*xrSpace, predictedEndTime);
    }

    void Session::setEyeTexturesToPresent(const Render::FrameResource& leftEye, const Render::FrameResource& rightEye) {
        this->leftEye = leftEye;
        this->rightEye = rightEye;

        auto& repo = vr.getEngine().getVulkanDriver().getResourceRepository();
        repo.getTextureUsages(leftEye.rootID) |= vk::ImageUsageFlagBits::eTransferSrc;
        repo.getTextureUsages(rightEye.rootID) |= vk::ImageUsageFlagBits::eTransferSrc;
    }

    void Session::startFrame() {
        if(!isReadyForRendering())
            return;
        xrWaitFrame();
        xrBeginFrame();
    }

    void Session::present(const Render::Context& regularRenderContext) {
        if(!isReadyForRendering())
            return;
/*        auto result = xrBeginFrame();
        if(result == xr::Result::FrameDiscarded) {
            return;
        }*/

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
            try {
                DISCARD(device.waitForFences(fence, true, UINT64_MAX));
            } catch(vk::DeviceLostError& deviceLost) {
                GetVulkanDriver().onDeviceLost();
            }
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
                    auto& texture = vr.getEngine().getVulkanDriver().getResourceRepository().getTexture(eyeTexture, swapchainIndex);
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
            vk::CommandBufferSubmitInfo commandBufferInfo {
                .commandBuffer = blitCommandBuffers[xrSwapchainIndex],
            };
            GetVulkanDriver().submitGraphics(vk::SubmitInfo2 {
                    .commandBufferInfoCount = 1,
                    .pCommandBufferInfos = &commandBufferInfo,
            }, *renderFences[xrSwapchainIndex]);
        }

        xrEndFrame();
    }

    void Session::attachActionSets() {
        xr::SessionActionSetsAttachInfo attachInfo;
        std::vector<xr::ActionSet> setsToAttach;

        auto& setList = Carrot::IO::ActionSet::getSetList();
        setsToAttach.reserve(setList.size());

        struct ProfileBindings {
            std::vector<xr::ActionSuggestedBinding> data;
        };
        std::unordered_map<std::string, ProfileBindings> perProfileBindings;
        auto updateActions = [&](const auto& vector, xr::ActionSet& xrActionSet) {
            for(auto& a : vector) {
                a->createXRAction(xrActionSet);

                for(const Carrot::IO::ActionBinding& binding : a->getSuggestedBindings()) {
                    if(!binding.isOpenXR()) {
                        continue;
                    }
                    xr::ActionSuggestedBinding xrBinding;
                    xrBinding.action = a->getXRAction();
                    xrBinding.binding = GetEngine().getVRSession().makeXRPath(binding.path);
                    perProfileBindings[binding.interactionProfile].data.push_back(xrBinding);
                }
            }
        };

        for(auto& actionSet : setList) {
            updateActions(actionSet->getBoolInputs(), actionSet->getXRActionSet());
            updateActions(actionSet->getFloatInputs(), actionSet->getXRActionSet());
            updateActions(actionSet->getVec2Inputs(), actionSet->getXRActionSet());
            updateActions(actionSet->getPoseInputs(), actionSet->getXRActionSet());
            updateActions(actionSet->getVibrationOutputs(), actionSet->getXRActionSet());

            for(auto* poseInput : actionSet->getPoseInputs()) {
                xr::ActionSpaceCreateInfo createInfo;
                createInfo.action = poseInput->getXRAction();
                poseInput->setXRSpace(xrSession->createActionSpaceUnique(createInfo));
            }

            setsToAttach.push_back(actionSet->getXRActionSet());
        }

        for(const auto& [profile, bindings] : perProfileBindings) {
            GetEngine().getVRSession().suggestBindings(profile, bindings.data);
        }

        if(setsToAttach.empty()) {
            return;
        }
        attachInfo.actionSets = setsToAttach.data();
        attachInfo.countActionSets = setsToAttach.size();
        xrSession->attachSessionActionSets(attachInfo);
    }

    void Session::syncActionSets(std::vector<Carrot::IO::ActionSet*> actionSets) {
        std::vector<xr::ActiveActionSet> xrSets;
        xrSets.reserve(actionSets.size());
        for(auto& set : actionSets) {
            xr::ActiveActionSet activeSet;
            activeSet.actionSet = set->getXRActionSet();
            activeSet.subactionPath = xr::Path::null();
            xrSets.emplace_back(activeSet);
        }
        xr::ActionsSyncInfo syncInfo;
        syncInfo.activeActionSets = xrSets.data();
        syncInfo.countActiveActionSets = xrSets.size();
        xrSession->syncActions(syncInfo);
    }

    void Session::vibrate(xr::Action& action, const xr::HapticVibration& vibrationDesc) {
        xr::HapticActionInfo info;
        info.action = action;

        xrSession->applyHapticFeedback(info, (const XrHapticBaseHeader*)&vibrationDesc);
    }

    void Session::stopVibration(xr::Action& action) {
        xr::HapticActionInfo info;
        info.action = action;
        xrSession->stopHapticFeedback(info);
    }

    xr::UniqueDynamicHandTrackerEXT Session::createHandTracker(const xr::HandTrackerCreateInfoEXT& createInfo) {
        return xrSession->createHandTrackerUniqueEXT(createInfo, vr.getXRDispatch());
    }

    xr::DispatchLoaderDynamic& Session::getXRDispatch() {
        return vr.getXRDispatch();
    }

    Carrot::Engine& Session::getEngine() {
        return vr.getEngine();
    }

    Session::~Session() {
        vr.unregisterSession(this);
    }

}