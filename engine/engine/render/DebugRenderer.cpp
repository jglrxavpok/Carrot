//
// Created by jglrxavpok on 20/07/25.
//

#include "DebugRenderer.h"

#include <engine/Engine.h>

#include "VulkanRenderer.h"
#include "resources/SingleMesh.h"

namespace Carrot::Render {
    DebugRenderer::DebugRenderer(VulkanRenderer& renderer): renderer(renderer) {
        lines.setGrowthFactor(1.5f);
        meshes.resize(MAX_FRAMES_IN_FLIGHT);
    }

    void DebugRenderer::drawLine(const glm::vec3& a, const glm::vec3& b, const glm::vec4& color) {
        LineDesc& desc = lines.emplaceBack();
        desc.a = a;
        desc.b = b;
        desc.color = color;
    }

    void DebugRenderer::drawLines(std::span<LineDesc> newLines) {
        const i64 start = lines.size();
        lines.resize(start + newLines.size());
        memcpy(lines.data() + start, newLines.data(), newLines.size()*sizeof(LineDesc));
    }

    void DebugRenderer::render(const Carrot::Render::Context& renderContext) {
        if (lines.empty()) {
            return;
        }

        if (!lineDrawPipeline) {
            lineDrawPipeline = renderer.getOrCreatePipeline("gBufferLines");
        }

        const u64 frameMod = renderContext.frameIndex;

        std::vector<Vertex> vertices;
        std::vector<u32> indices;

        vertices.resize(lines.size() * 2);
        indices.resize(lines.size() * 2);

        // parallel?
        for (i32 vertexIndex = 0; vertexIndex < vertices.size(); vertexIndex += 2) {
            vertices[vertexIndex + 0].pos = glm::vec4{ lines[vertexIndex/2].a, 1 };
            vertices[vertexIndex + 1].pos = glm::vec4{ lines[vertexIndex/2].b, 1 };

            vertices[vertexIndex + 0].color = lines[vertexIndex/2].color;
            vertices[vertexIndex + 1].color = lines[vertexIndex/2].color;

            indices[vertexIndex + 0] = vertexIndex + 0;
            indices[vertexIndex + 1] = vertexIndex + 1;
        }
        // TODO: do we really need indices? Needs something else than SingleMesh
        std::shared_ptr<SingleMesh>& pMesh = meshes[frameMod].emplaceBack();
        pMesh = std::make_shared<SingleMesh>(vertices, indices);

        SingleMesh& mesh = *pMesh;

        Carrot::InstanceData instanceData{};
        Carrot::GBufferDrawData data{};

        data.materialIndex = GetRenderer().getWhiteMaterial().getSlot();
        auto& renderPacket = renderer.makeRenderPacket(PassEnum::OpaqueGBuffer, PacketType::DrawIndexedInstanced, renderContext);
        renderPacket.addPerDrawData({&data, 1});
        renderPacket.pipeline = lineDrawPipeline;
        renderPacket.useMesh(mesh);
        renderPacket.useInstance(instanceData);

        renderer.render(renderPacket);

        if (renderContext.pViewport == &renderer.getEngine().getMainViewport()) {
            u64 nextFrameMod = (renderContext.frameCount+1) % meshes.size();
            meshes[nextFrameMod].clear();
            lines.clear();
            return;
        }
    }

} // Carrot::Render