//
// Created by jglrxavpok on 26/06/2023.
//

#include "JoltDebugRenderer.h"
#include <engine/Engine.h>
#include <engine/render/VulkanRenderer.h>
#include <engine/render/Viewport.h>
#include <engine/render/InstanceData.h>
#include <engine/utils/conversions.h>
#include <engine/render/resources/SingleMesh.h>

namespace Carrot::Physics {
    struct TriangleMesh: public JPH::RefTargetVirtual {
        std::shared_ptr<Carrot::Mesh> mesh;

        virtual void AddRef() override			{ ++mRefCount; }
        virtual void Release() override			{ if (--mRefCount == 0) delete this; }

        std::atomic<std::uint32_t> mRefCount = 0;
    };

    JoltDebugRenderer::JoltDebugRenderer(Carrot::Render::Viewport& debugViewport): debugViewport(debugViewport) {
        debugTrianglesPipeline = GetRenderer().getOrCreatePipeline("unlitGBuffer");

        Initialize();
    }

    void JoltDebugRenderer::render(const Carrot::Render::Context& renderContext) {
        if(vertices.empty()) {
            return;
        }

        Carrot::Render::Packet& renderPacket = GetRenderer().makeRenderPacket(Carrot::Render::PassEnum::OpaqueGBuffer, Render::PacketType::DrawIndexedInstanced, debugViewport);
        renderPacket.pipeline = debugTrianglesPipeline;

        Carrot::BufferView vertexBuffer = GetRenderer().getSingleFrameHostBuffer(vertices.size() * sizeof(Carrot::Vertex));
        Carrot::BufferView indexBuffer = GetRenderer().getSingleFrameHostBuffer(indices.size() * sizeof(std::uint32_t));

        vertexBuffer.directUpload(std::span<const Carrot::Vertex>{vertices});
        indexBuffer.directUpload(std::span<const std::uint32_t>{indices});

        renderPacket.vertexBuffer = vertexBuffer;
        renderPacket.indexBuffer = indexBuffer;

        auto& cmd = renderPacket.commands.emplace_back().drawIndexedInstanced;
        cmd.indexCount = indices.size();
        cmd.instanceCount = 1;

        Carrot::GBufferDrawData drawData;
        drawData.materialIndex = GetRenderer().getWhiteMaterial().getSlot();

        Carrot::InstanceData instance;
        renderPacket.useInstance(instance);
        renderPacket.addPerDrawData({&drawData, 1});

        GetRenderer().render(renderPacket);

        vertices.clear();
        indices.clear();
    }

    void JoltDebugRenderer::DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor) {

    }

    void
    JoltDebugRenderer::DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor, ECastShadow inCastShadow /*unused*/) {
        std::size_t index = vertices.size();
        vertices.resize(vertices.size() + 3);
        indices.resize(indices.size() + 3);

        vertices[index + 0].pos = glm::vec4{ joltToCarrot(inV1), 1.0f};
        vertices[index + 0].color = joltToCarrot(inColor.ToVec4());

        vertices[index + 1].pos = glm::vec4{ joltToCarrot(inV2), 1.0f};
        vertices[index + 1].color = joltToCarrot(inColor.ToVec4());

        vertices[index + 2].pos = glm::vec4{ joltToCarrot(inV3), 1.0f};
        vertices[index + 2].color = joltToCarrot(inColor.ToVec4());

        for (int i = 0; i < 3; ++i) {
            indices[index + i] = index + i;
        }
    }

    JPH::DebugRenderer::Batch
    JoltDebugRenderer::CreateTriangleBatch(const DebugRenderer::Triangle *inTriangles, int inTriangleCount) {
        std::vector<Carrot::Vertex> batchVertices;
        std::vector<std::uint32_t> batchIndices;
        batchVertices.resize(inTriangleCount * 3);
        batchIndices.resize(inTriangleCount * 3);

        for (int i = 0; i < inTriangleCount; ++i) {
            for (int j = 0; j < 3; ++j) {
                auto& joltVertex = inTriangles[i].mV[j];
                auto& carrotVertex = batchVertices[i * 3 + j];

                carrotVertex.pos = glm::vec4{ joltToCarrot(joltVertex.mPosition), 1.0f };
                carrotVertex.color = joltToCarrot(joltVertex.mColor.ToVec4());
                carrotVertex.normal = joltToCarrot(joltVertex.mNormal);
                carrotVertex.uv = joltToCarrot(joltVertex.mUV);

                batchIndices[i * 3 + j] = i * 3 + j;
            }
        }

        std::shared_ptr<Carrot::Mesh> mesh = std::make_unique<Carrot::SingleMesh>(batchVertices, batchIndices);

        auto* triangleMesh = new TriangleMesh;
        triangleMesh->mesh = mesh;
        return {triangleMesh};
    }

    JPH::DebugRenderer::Batch JoltDebugRenderer::CreateTriangleBatch(const DebugRenderer::Vertex *inVertices, int inVertexCount,
                                                            const JPH::uint32 *inIndices, int inIndexCount) {
        std::vector<Carrot::Vertex> batchVertices;
        std::vector<std::uint32_t> batchIndices;
        batchVertices.resize(inVertexCount);
        batchIndices.resize(inIndexCount);

        for (int i = 0; i < inVertexCount; ++i) {
            auto& joltVertex = inVertices[i];
            auto& carrotVertex = batchVertices[i];

            carrotVertex.pos = glm::vec4{ joltToCarrot(joltVertex.mPosition), 1.0f };
            carrotVertex.color = joltToCarrot(joltVertex.mColor.ToVec4());
            carrotVertex.normal = joltToCarrot(joltVertex.mNormal);
            carrotVertex.uv = joltToCarrot(joltVertex.mUV);
        }

        memcpy(batchIndices.data(), inIndices, inIndexCount * sizeof(JPH::uint32));

        std::shared_ptr<Carrot::Mesh> mesh = std::make_unique<Carrot::SingleMesh>(batchVertices, batchIndices);

        auto* triangleMesh = new TriangleMesh;
        triangleMesh->mesh = mesh;
        return {triangleMesh};
    }

    void JoltDebugRenderer::DrawGeometry(const JPH::Mat44& inModelMatrix, const JPH::AABox& inWorldSpaceBounds,
                                     float inLODScaleSq, JPH::ColorArg inModelColor,
                                     const DebugRenderer::GeometryRef& inGeometry, DebugRenderer::ECullMode inCullMode,
                                     DebugRenderer::ECastShadow inCastShadow, DebugRenderer::EDrawMode inDrawMode) {
        if(!inGeometry) {
            return;
        }

        if(inGeometry->mLODs.empty()) {
            return;
        }

        // TODO: lod selection
        // TODO: handle shadow/cull/draw modes
        TriangleMesh* triangleMesh = (TriangleMesh*)inGeometry->mLODs[0].mTriangleBatch.GetPtr();
        Carrot::Render::Packet& renderPacket = GetRenderer().makeRenderPacket(Carrot::Render::PassEnum::Unlit, Render::PacketType::DrawIndexedInstanced, debugViewport);

        renderPacket.pipeline = debugTrianglesPipeline;
        renderPacket.useMesh(*triangleMesh->mesh);

        Carrot::GBufferDrawData drawData;
        drawData.materialIndex = GetRenderer().getWhiteMaterial().getSlot();

        Carrot::InstanceData instance;
        instance.color = joltToCarrot(inModelColor.ToVec4());
        instance.transform = joltToCarrot(inModelMatrix);
        renderPacket.useInstance(instance);
        renderPacket.addPerDrawData({&drawData, 1});

        GetRenderer().render(renderPacket);
    }

    void JoltDebugRenderer::DrawText3D(JPH::RVec3Arg inPosition, const std::string_view& inString, JPH::ColorArg inColor,
                                   float inHeight) {

    }
} // Carrot::Physics