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
        size_t frameCount = -1;
        u8 frameIndex = -1; // TODO: only use frameCount?
        size_t swapchainImageIndex = -1;

        u8 getPreviousFrameIndex() const;

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