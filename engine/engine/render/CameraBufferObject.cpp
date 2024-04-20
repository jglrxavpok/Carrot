//
// Created by jglrxavpok on 10/05/2023.
//

#include <engine/render/CameraBufferObject.h>
#include <engine/render/RenderContext.h>
#include <engine/utils/Macros.h>
#include <engine/Engine.h>
#include <engine/render/VulkanRenderer.h>

namespace Carrot {
    void CameraBufferObject::update(Camera& camera, const Render::Context& renderContext) {
        view = camera.getCurrentFrameViewMatrix();
        inverseView = glm::inverse(view);
        nonJitteredProjection = camera.getCurrentFrameProjectionMatrix();

        const std::uint32_t frameCount = GetRenderer().getFrameCount();
        const glm::vec2 translationAmount =
                (glm::vec2((frameCount % 2) - 0.5, ((frameCount / 2) % 2) - 0.5) / renderContext.pViewport->getSizef())
                * 0.25f;
        jitteredProjection = nonJitteredProjection;
        jitteredProjection[3][0] += translationAmount.x;
        jitteredProjection[3][1] += translationAmount.y;

        inverseJitteredProjection = glm::inverse(jitteredProjection);
        inverseNonJitteredProjection = glm::inverse(nonJitteredProjection);

        for (int i = 0; i < 6; ++i) {
            frustum[i] = camera.getFrustumPlane(i);
        }
    }
}