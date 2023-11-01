//
// Created by jglrxavpok on 15/11/2021.
//

#include "BillboardRenderer.h"
#include "engine/Engine.h"
#include "engine/render/VulkanRenderer.h"
#include "engine/render/resources/Mesh.h"
#include "engine/render/RenderPacket.h"

namespace Carrot::Render {
    BillboardRenderer::BillboardRenderer() {
        pipeline = GetRenderer().getOrCreatePipeline("billboards");
    }

    void BillboardRenderer::render(const glm::vec3& position, float scale, const TextureHandle& texture, const Carrot::Render::Context& renderContext, const glm::vec3& color, const Carrot::UUID& uuid) {
        // TODO: this push constant prevents merging render packets, modify pipeline to allow to use instance buffer instead
        struct {
            glm::vec3 pos{0.0f};
            float scale = 1.0f;
            glm::uvec4 uuid;
            glm::vec3 color;
            std::uint32_t textureID = -1;
        } pushConstant;
        static_assert(sizeof(pushConstant) == 48, "Must be same size than inside shader");
        pushConstant.pos = position;
        pushConstant.scale = scale;
        pushConstant.uuid = { uuid.data0(), uuid.data1(), uuid.data2(), uuid.data3() };
        pushConstant.textureID = texture.getSlot();
        pushConstant.color = color;

        Render::Packet& packet = GetRenderer().makeRenderPacket(Render::PassEnum::OpaqueGBuffer, renderContext);
        packet.pipeline = pipeline;
        packet.useMesh(GetRenderer().getFullscreenQuad());

        auto& billboardPushConstant = packet.addPushConstant("billboard", vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);
        billboardPushConstant.setData(pushConstant);

        renderContext.renderer.render(std::move(packet));
    }
}