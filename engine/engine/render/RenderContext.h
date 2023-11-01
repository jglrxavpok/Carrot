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
        size_t swapchainIndex = -1;
        size_t lastSwapchainIndex = -1;

        void copyFrom(const Context& other);

        //! Camera currently used for rendering. It is valid to modify it during rendering
        Carrot::Camera& getCamera();

        //! Camera currently used for rendering
        const Carrot::Camera& getCamera() const;

        vk::DescriptorSet getCameraDescriptorSet() const;

        /// Returns a copy of this RenderContext, with swapchainIndex = lastSwapchainIndex. Calling it again on the
        /// resulting object won't have any effect (outside of creating a new object)
        Context lastFrame() const;

        void renderWireframeSphere(const glm::mat4& transform, float radius, const glm::vec4& color, const Carrot::UUID& objectID = Carrot::UUID::null());
        void renderWireframeCuboid(const glm::mat4& transform, const glm::vec3& halfExtents, const glm::vec4& color, const Carrot::UUID& objectID = Carrot::UUID::null());
        void renderWireframeCapsule(const glm::mat4& transform, float radius, float height, const glm::vec4& color, const Carrot::UUID& objectID = Carrot::UUID::null());
        void render(const Render::Packet& packet);

        static void registerUsertype(sol::state& destination);
    };
}