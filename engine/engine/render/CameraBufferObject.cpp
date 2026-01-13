//
// Created by jglrxavpok on 10/05/2023.
//

#include <engine/render/CameraBufferObject.h>
#include <engine/render/RenderContext.h>
#include <engine/utils/Macros.h>
#include <engine/Engine.h>
#include <engine/render/VulkanRenderer.h>

namespace Carrot {

    glm::vec2 r2Sequence(std::size_t n) {
        const float g = 1.32471795724474602596f;
        const float a1 = 1.0f / g;
        const float a2 = 1.0f / (g*g);
        return glm::fract(glm::vec2((0.5f+a1*n), (0.5f+a2*n)));
    }

    void CameraBufferObject::update(Camera& camera, const Render::Context& renderContext) {
        view = camera.getCurrentFrameViewMatrix();
        inverseView = glm::inverse(view);
        nonJitteredProjection = camera.getCurrentFrameProjectionMatrix();

        const std::uint32_t frameCount = GetRenderer().getFrameCount();
        const glm::vec2 translationAmount =
                /*(glm::vec2((frameCount % 2) - 0.5, ((frameCount / 2) % 2) - 0.5) / renderContext.pViewport->getSizef())
                * 0.25f;*/
                    (r2Sequence(frameCount) - 0.5f) / renderContext.pViewport->getSizef() * 2.0f;
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