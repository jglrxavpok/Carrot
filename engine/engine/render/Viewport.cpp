//
// Created by jglrxavpok on 22/09/2021.
//

#include "Viewport.h"
#include "core/Macros.h"
#include "engine/render/RenderContext.h"
#include "engine/render/VulkanRenderer.h"
#include "engine/render/CameraBufferObject.h"
#include "engine/render/resources/ResourceAllocator.h"
#include "engine/Engine.h"
#include "engine/scene/Scene.h"
#include "engine/vr/Session.h"
#include "ViewportBufferObject.h"

namespace Carrot::Render {
    Viewport::Viewport(VulkanRenderer& renderer): renderer(renderer) {
        onSwapchainImageCountChange(renderer.getSwapchainImageCount());
    }

    Viewport::~Viewport() {
        for(auto& pScene : scenes) {
            pScene->unbindFromViewport(*this);
        }
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
        if(!cameraDescriptorSets.empty()) {
            renderer.destroyCameraDescriptorSets(cameraDescriptorSets);
        }

        if(!viewportDescriptorSets.empty()) {
            renderer.destroyViewportDescriptorSets(viewportDescriptorSets);
        }

        std::size_t totalCount = newCount * (renderer.getConfiguration().runInVR ? 2 : 1);
        cameraUniformBuffers.resize(totalCount);
        viewportUniformBuffers.resize(newCount);
        for (std::size_t i = 0; i < totalCount; i++) {
            cameraUniformBuffers[i] = renderer.getEngine().getResourceAllocator().allocateBuffer(sizeof(Carrot::CameraBufferObject),
                                                                                       vk::BufferUsageFlagBits::eUniformBuffer,
                                                                                       vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        }

        for(std::size_t i = 0; i < newCount; i++) {
            viewportUniformBuffers[i] = renderer.getEngine().getResourceAllocator().allocateBuffer(sizeof(Carrot::ViewportBufferObject),
                                                                                                 vk::BufferUsageFlagBits::eUniformBuffer,
                                                                                                 vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        }
        cameraDescriptorSets = renderer.createDescriptorSetForCamera(cameraUniformBuffers);
        viewportDescriptorSets = renderer.createDescriptorSetForViewport(viewportUniformBuffers);

        if(renderGraph) {
            renderGraph->onSwapchainImageCountChange(newCount);
        }
    }

    void Viewport::onSwapchainSizeChange(int newWidth, int newHeight) {
        if(followSwapchainSize) {
            resize(newWidth, newHeight);
            followSwapchainSize = true;
        }
    }

    vk::DescriptorSet Viewport::getCameraDescriptorSet(const Carrot::Render::Context& context) const {
        if(context.renderer.getConfiguration().runInVR) {
            return cameraDescriptorSets[context.swapchainIndex * 2 + (context.eye == Eye::RightEye ? 1 : 0)];
        } else {
            return cameraDescriptorSets[context.swapchainIndex];
        }
    }

    vk::DescriptorSet Viewport::getViewportDescriptorSet(const Carrot::Render::Context& context) const {
        return viewportDescriptorSets[context.swapchainIndex];
    }

    void Viewport::onFrame(const Context& context) {
        verify(context.pViewport == this, "Viewports must match!");
        if(context.renderer.getConfiguration().runInVR && vrCompatible) {
            CameraBufferObject objLeftEye{};
            CameraBufferObject objRightEye{};

            Camera leftEyeCamera = getCamera(Carrot::Render::Eye::LeftEye);
            Camera rightEyeCamera = getCamera(Carrot::Render::Eye::RightEye);

            leftEyeCamera.getViewMatrixRef() = GetEngine().getVRSession().getEyeView(Carrot::Render::Eye::LeftEye) * leftEyeCamera.getViewMatrixRef();
            leftEyeCamera.getProjectionMatrixRef() = GetEngine().getVRSession().getEyeProjection(Carrot::Render::Eye::LeftEye);

            rightEyeCamera.getViewMatrixRef() = GetEngine().getVRSession().getEyeView(Carrot::Render::Eye::RightEye) * rightEyeCamera.getViewMatrixRef();
            rightEyeCamera.getProjectionMatrixRef() = GetEngine().getVRSession().getEyeProjection(Carrot::Render::Eye::RightEye);

            objLeftEye.update(leftEyeCamera, context);
            objRightEye.update(rightEyeCamera, context);

            auto& leftBuffer = cameraUniformBuffers[context.swapchainIndex * 2];
            auto& rightBuffer = cameraUniformBuffers[context.swapchainIndex * 2 + 1];
            leftBuffer.getBuffer().directUpload(&objLeftEye, sizeof(objLeftEye), leftBuffer.getStart());
            rightBuffer.getBuffer().directUpload(&objRightEye, sizeof(objRightEye), rightBuffer.getStart());
        } else {
            CameraBufferObject obj{};
            auto& buffer = cameraUniformBuffers[context.swapchainIndex];

            obj.update(getCamera(), context);

            buffer.getBuffer().directUpload(&obj, sizeof(obj), buffer.getStart());
        }

        ViewportBufferObject viewportBufferObject{};
        viewportBufferObject.update(*this);
        auto& buffer = viewportUniformBuffers[context.swapchainIndex];
        buffer.getBuffer().directUpload(&viewportBufferObject, sizeof(viewportBufferObject), buffer.getStart());

        for(auto& pScene : scenes) {
            pScene->onFrame(context);
        }
        if(renderGraph) {
            renderGraph->onFrame(context);
        }
    }

    const Carrot::BufferView& Viewport::getCameraUniformBuffer(const Render::Context& context) const {
        if(context.renderer.getConfiguration().runInVR && vrCompatible) {
            return cameraUniformBuffers[context.swapchainIndex * 2 + (context.eye == Eye::RightEye ? 1 : 0)];
        }
        return cameraUniformBuffers[context.swapchainIndex];
    }

    const Carrot::BufferView& Viewport::getViewportUniformBuffer(const Render::Context& context) const {
        return viewportUniformBuffers[context.swapchainIndex];
    }

    void Viewport::render(const Carrot::Render::Context& context, vk::CommandBuffer& cmds) {
        if(renderGraph) {
            renderGraph->execute(context, cmds);
        }
    }

    void Viewport::addScene(Scene* pScene) {
        verify(pScene != nullptr, "Cannot add null scene");
        scenes.emplace_back(pScene);
    }

    void Viewport::removeScene(Scene* pScene) {
        verify(pScene != nullptr, "Cannot remove null scene");
        std::erase(scenes, pScene);
    }

    Render::Graph* Viewport::getRenderGraph() {
        return renderGraph.get();
    }

    void Viewport::setRenderGraph(std::unique_ptr<Render::Graph>&& renderGraph) {
        this->renderGraph = std::move(renderGraph);
    }

    void Viewport::resize(std::uint32_t w, std::uint32_t h) {
        this->width = w;
        this->height = h;
        if(renderGraph) {
            renderGraph->onSwapchainSizeChange(w, h);
        }
        followSwapchainSize = false;
    }

    std::uint32_t Viewport::getWidth() const {
        if(followSwapchainSize) {
            return GetVulkanDriver().getFinalRenderSize().width;
        } else {
            return width;
        }
    }

    std::uint32_t Viewport::getHeight() const {
        if(followSwapchainSize) {
            return GetVulkanDriver().getFinalRenderSize().height;
        } else {
            return height;
        }
    }

    glm::vec2 Viewport::getSizef() const {
        return glm::vec2 { getWidth(), getHeight() };
    }

    glm::vec2 Viewport::getOffset() const {
        return offset;
    }

    void Viewport::setOffset(const glm::vec2& offset) {
        this->offset = offset;
    }
}