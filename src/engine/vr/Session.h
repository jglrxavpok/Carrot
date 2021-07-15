//
// Created by jglrxavpok on 13/07/2021.
//
#pragma once
#ifdef ENABLE_VR

#include "engine/vr/includes.h"
#include "engine/vulkan/includes.h"
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

        void waitFrame();
        void beginFrame();
        void endFrame();

    public:
        void setEyeTexturesToPresent(const Render::FrameResource& leftEye, const Render::FrameResource& rightEye);
        void present(const Render::Context& context);

    private:
        xr::Session& getXRSession() { return *xrSession; }
        Carrot::Engine& getEngine();

    private:
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
        vk::Extent3D fullSwapchainSize;
        vk::Extent3D eyeRenderSize;
        vk::Format swapchainFormat = vk::Format::eUndefined;
        xr::UniqueSwapchain xrSwapchain;
        std::vector<xr::SwapchainImageVulkanKHR> xrSwapchainImages;
        std::vector<Carrot::Render::Texture::Ref> xrSwapchainTextures;
        std::uint32_t xrSwapchainIndex = -1;

        Render::FrameResource leftEye;
        Render::FrameResource rightEye;

        friend class Interface;
    };
}
#endif