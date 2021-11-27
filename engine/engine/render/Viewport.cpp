//
// Created by jglrxavpok on 22/09/2021.
//

#include "Viewport.h"
#include "core/Macros.h"
#include "engine/render/VulkanRenderer.h"
#include "engine/render/CameraBufferObject.h"
#include "engine/render/resources/ResourceAllocator.h"
#include "engine/Engine.h"

namespace Carrot::Render {
    Viewport::Viewport(VulkanRenderer& renderer): renderer(renderer) {
        onSwapchainImageCountChange(renderer.getSwapchainImageCount());
    }

    Carrot::Camera& Viewport::getCamera(Carrot::Render::Eye eye) {
        return cameras[eye];
    }

    const Carrot::Camera& Viewport::getCamera(Carrot::Render::Eye eye) const {
        auto it = cameras.find(eye);
        verify(it != cameras.end(), "Camera does not exist");
        return it->second;
    }

    void Viewport::onSwapchainImageCountChange(size_t newCount) {
        if(cameraDescriptorSets.empty()) {
            auto& device = renderer.getVulkanDriver().getLogicalDevice();
            renderer.destroyCameraDescriptorSets(cameraDescriptorSets);
        }

        std::size_t totalCount = newCount * (renderer.getConfiguration().runInVR ? 2 : 1);
        cameraUniformBuffers.resize(totalCount);
        for (int i = 0; i < totalCount; i++) {
            cameraUniformBuffers[i] = renderer.getEngine().getResourceAllocator().allocateBuffer(sizeof(Carrot::CameraBufferObject),
                                                                                       vk::BufferUsageFlagBits::eUniformBuffer,
                                                                                       vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        }
        cameraDescriptorSets = renderer.createDescriptorSetForCamera(cameraUniformBuffers);
        renderer.createDescriptorSetForCamera(cameraUniformBuffers);
    }

    void Viewport::onSwapchainSizeChange(int newWidth, int newHeight) {
        // no op
    }

    vk::DescriptorSet Viewport::getCameraDescriptorSet(const Carrot::Render::Context& context) const {
        if(context.renderer.getConfiguration().runInVR) {
            return cameraDescriptorSets[context.swapchainIndex * 2 + (context.eye == Eye::RightEye ? 1 : 0)];
        } else {
            return cameraDescriptorSets[context.swapchainIndex];
        }
    }

    void Viewport::onFrame(const Context& context) {
        verify(&context.viewport == this, "Viewports must match!");
        if(context.renderer.getConfiguration().runInVR) {
            static CameraBufferObject objLeftEye{};
            static CameraBufferObject objRightEye{};

            objLeftEye.update(getCamera(Carrot::Render::Eye::LeftEye));
            objRightEye.update(getCamera(Carrot::Render::Eye::RightEye));

            auto& leftBuffer = cameraUniformBuffers[context.swapchainIndex * 2];
            auto& rightBuffer = cameraUniformBuffers[context.swapchainIndex * 2 + 1];
            leftBuffer.getBuffer().directUpload(&objLeftEye, sizeof(objLeftEye), leftBuffer.getStart());
            rightBuffer.getBuffer().directUpload(&objRightEye, sizeof(objRightEye), rightBuffer.getStart());
        } else {
            static CameraBufferObject obj{};
            auto& buffer = cameraUniformBuffers[context.swapchainIndex];

            obj.update(getCamera());

            buffer.getBuffer().directUpload(&obj, sizeof(obj), buffer.getStart());
        }
    }
}