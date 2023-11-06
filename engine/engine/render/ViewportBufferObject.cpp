//
// Created by jglrxavpok on 15/04/2023.
//

#include <engine/render/ViewportBufferObject.h>
#include <engine/render/Viewport.h>
#include <engine/Engine.h>
#include <engine/render/VulkanRenderer.h>
#include <engine/render/raytracing/ASBuilder.h>

namespace Carrot {
    void ViewportBufferObject::update(const Render::Context& renderContext) {
        frameWidth = renderContext.pViewport->getWidth();
        frameHeight = renderContext.pViewport->getHeight();
        frameCount = GetRenderer().getFrameCount();

        if(GetEngine().getCapabilities().supportsRaytracing) {
            hasTLAS = GetRenderer().getASBuilder().getTopLevelAS(renderContext) != nullptr;
        }
    }
}