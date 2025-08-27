//
// Created by jglrxavpok on 20/06/2021.
//

#pragma once

#include "RenderEye.h"
#include "Viewport.h"
#include "core/utils/UUID.h"

namespace sol {
    class state;
}

namespace Carrot {
    class VulkanRenderer;
}

namespace Carrot::Render {
    class Packet;

    struct Context {
        VulkanRenderer& renderer;
        Viewport* pViewport = nullptr;
        Eye eye = Eye::NoVR;
        u64 frameNumber = -1; // TODO: rename to frame number
        u8 frameIndex = -1; // frameCount % MAX_FRAMES_IN_FLIGHT
        size_t swapchainImageIndex = -1;

        u8 getPreviousFrameIndex() const;

        //! Gets the frame number used to reference resources from the previous frame.
        //! Technically this frame number is lying to you on the first frame, it will return a frame in the future.
        //! The intent is that this is used for ring buffers/textures with a length of MAX_FRAMES_IN_FLIGHT in order to
        //! reference valid resources even on the first frame
        u64 getPreviousFrameNumber() const;

        //! Gets the frame number used to reference resources from the previous frame.
        //! If the current frame number is 0, this will also return 0.
        u64 getRealPreviousFrameNumber() const;

        void copyFrom(const Context& other);

        //! Camera currently used for rendering. It is valid to modify it during rendering
        Carrot::Camera& getCamera();

        //! Camera currently used for rendering
        const Carrot::Camera& getCamera() const;

        vk::DescriptorSet getCameraDescriptorSet() const;

        void renderWireframeSphere(const glm::mat4& transform, float radius, const glm::vec4& color, const Carrot::UUID& objectID = Carrot::UUID::null());
        void renderWireframeCuboid(const glm::mat4& transform, const glm::vec3& halfExtents, const glm::vec4& color, const Carrot::UUID& objectID = Carrot::UUID::null());
        void renderWireframeCapsule(const glm::mat4& transform, float radius, float height, const glm::vec4& color, const Carrot::UUID& objectID = Carrot::UUID::null());
        void render(const Render::Packet& packet);
    };
}