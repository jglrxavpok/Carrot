//
// Created by jglrxavpok on 15/11/2021.
//

#include "BillboardRenderer.h"
#include "engine/Engine.h"
#include "engine/render/VulkanRenderer.h"
#include "engine/render/resources/Mesh.h"

namespace Carrot::Render {
    BillboardRenderer::BillboardRenderer() {
        pipeline = GetRenderer().getOrCreatePipeline("billboards");
    }

    void BillboardRenderer::gBufferDraw(const glm::vec3& position, float scale, const TextureHandle& texture, vk::RenderPass pass, const Carrot::Render::Context& renderContext, vk::CommandBuffer& cmds, const glm::vec3& color, const Carrot::UUID& uuid) {
        struct {
            glm::vec3 pos;
            float scale;
            glm::uvec4 uuid;
            glm::vec3 color;
            std::uint32_t textureID;
        } pushConstant;
        static_assert(sizeof(pushConstant) == 48, "Must be same size than inside shader");
        pushConstant.pos = position;
        pushConstant.scale = scale;
        pushConstant.uuid = { uuid.data0(), uuid.data1(), uuid.data2(), uuid.data3() };
        pushConstant.textureID = texture.getSlot();
        pushConstant.color = color;

        pipeline->bind(pass, renderContext, cmds);
        GetRenderer().pushConstantBlock("billboard", *pipeline, renderContext, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, cmds, pushConstant);

        auto& mesh = GetRenderer().getFullscreenQuad();
        mesh.bind(cmds);
        mesh.draw(cmds);
    }
}