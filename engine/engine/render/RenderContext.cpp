//
// Created by jglrxavpok on 22/09/2021.
//

#include "RenderContext.h"
#include "Viewport.h"
#include "VulkanRenderer.h"

namespace Carrot::Render {
    void Context::copyFrom(const Context& other) {
        eye = other.eye;
        frameCount = other.frameCount;
        swapchainIndex = other.swapchainIndex;
        lastSwapchainIndex = other.lastSwapchainIndex;
    }

    Carrot::Camera& Context::getCamera() {
        return pViewport->getCamera(eye);
    }

    const Carrot::Camera& Context::getCamera() const {
        return pViewport->getCamera(eye);
    }

    vk::DescriptorSet Context::getCameraDescriptorSet() const {
        return pViewport->getCameraDescriptorSet(*this);
    }

    Context Context::lastFrame() const {
        return Context {
            .renderer = renderer,
            .pViewport = pViewport,
            .eye = eye,
            .frameCount = frameCount,
            .swapchainIndex = lastSwapchainIndex, // <-- only change
            .lastSwapchainIndex = lastSwapchainIndex,
        };
    }

    void Context::renderWireframeSphere(const glm::mat4& transform, float radius, const glm::vec4& color, const Carrot::UUID& objectID) {
        renderer.renderWireframeSphere(*this, transform, radius, color, objectID);
    }

    void Context::renderWireframeCuboid(const glm::mat4& transform, const glm::vec3& halfExtents, const glm::vec4& color, const Carrot::UUID& objectID) {
        renderer.renderWireframeCuboid(*this, transform, halfExtents, color, objectID);
    }

    void Context::renderWireframeCapsule(const glm::mat4& transform, float radius, float height, const glm::vec4& color, const Carrot::UUID& objectID) {
        renderer.renderWireframeCapsule(*this, transform, radius, height, color, objectID);
    }

    void Context::render(const Render::Packet& packet) {
        renderer.render(packet);
    }
}