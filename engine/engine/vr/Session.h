//
// Created by jglrxavpok on 13/07/2021.
//
#pragma once

#include "engine/vulkan/includes.h"
#include "engine/vr/includes.h"
#include "engine/render/resources/Texture.h"
#include "engine/render/RenderGraph.h"
#include <memory>

namespace Carrot {
    class Engine;
}

namespace Carrot::VR {
    class Interface;

    class Session {
    public:
        explicit Session(Interface& vr);

        ~Session();

    public:
        bool isReadyForRendering() const { return readyForRendering; }
        bool shouldRenderToSwapchain() const { return shouldRender; }
        const vk::Extent2D& getEyeRenderSize() const { return eyeRenderSize; }

    public:
        void setEyeTexturesToPresent(const Render::FrameResource& leftEye, const Render::FrameResource& rightEye);

        void startFrame();
        void present(const Render::Context& context);

    private:
        xr::Session& getXRSession() { return *xrSession; }
        Carrot::Engine& getEngine();

    private:
        void xrWaitFrame();
        xr::Result xrBeginFrame();
        void xrEndFrame();

        void stateChanged(xr::Time time, xr::SessionState state);

    private:
        Interface& vr;
        xr::UniqueSession xrSession;
        bool readyForRendering = false;
        bool shouldRender = false;
        xr::Time predictedEndTime;
        xr::UniqueSpace xrSpace;
        std::vector<xr::View> xrViews;
        xr::CompositionLayerProjectionView xrProjectionViews[2];

    private: // swapchain
        vk::Extent2D fullSwapchainSize;
        vk::Extent2D eyeRenderSize;
        vk::Format swapchainFormat = vk::Format::eUndefined;
        xr::UniqueSwapchain xrSwapchain;
        std::vector<xr::SwapchainImageVulkanKHR> xrSwapchainImages;
        std::vector<Carrot::Render::Texture::Ref> xrSwapchainTextures;
        std::uint32_t xrSwapchainIndex = -1;

        Render::FrameResource leftEye;
        Render::FrameResource rightEye;

    private: // pre-record blit
        vk::UniqueCommandPool blitCommandPool;
        std::vector<vk::CommandBuffer> blitCommandBuffers;
        std::vector<vk::UniqueFence> renderFences;

        friend class Interface;
    };
}