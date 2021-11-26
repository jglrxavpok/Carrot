//
// Created by jglrxavpok on 15/11/2021.
//

#pragma once

#include <glm/glm.hpp>
#include "engine/render/MaterialSystem.h"
#include "engine/render/resources/Pipeline.h"
#include "engine/utils/UUID.h"

namespace Carrot::Render {
    class BillboardRenderer {
    public:
        explicit BillboardRenderer();

        void gBufferDraw(const glm::vec3& position,
                  float scale,
                  const TextureHandle& texture,
                  vk::RenderPass pass,
                  const Carrot::Render::Context& renderContext,
                  vk::CommandBuffer& cmds,
                  const glm::vec3& color = glm::vec3{1.0f},
                  const Carrot::UUID& uuid = Carrot::UUID::null()
                  );

    private:
        std::shared_ptr<Carrot::Pipeline> pipeline;
    };
}
