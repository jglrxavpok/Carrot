//
// Created by jglrxavpok on 22/09/2021.
//

#include "RenderContext.h"
#include "Viewport.h"
#include "VulkanRenderer.h"

namespace Carrot::Render {
    vk::DescriptorSet Context::getCameraDescriptorSet() const {
        return viewport.getCameraDescriptorSet(*this);
    }

    Context Context::lastFrame() const {
        return Context {
            .renderer = renderer,
            .viewport = viewport,
            .eye = eye,
            .frameCount = frameCount,
            .swapchainIndex = lastSwapchainIndex, // <-- only change
            .lastSwapchainIndex = lastSwapchainIndex,
        };
    }

    void Context::renderWireframeSphere(const glm::vec3& position, float radius, const glm::vec4& color, const Carrot::UUID& objectID) {
        renderer.renderWireframeSphere(*this, position, radius, color, objectID);
    }

    void Context::renderWireframeCuboid(const glm::vec3& position, const glm::vec3& halfExtents, const glm::vec4& color, const Carrot::UUID& objectID) {
        renderer.renderWireframeCuboid(*this, position, halfExtents, color, objectID);
    }

    void Context::renderWireframeCapsule(const glm::vec3& position, float radius, float height, const glm::vec4& color, const Carrot::UUID& objectID) {
        renderer.renderWireframeCapsule(*this, position, radius, height, color, objectID);
    }
}