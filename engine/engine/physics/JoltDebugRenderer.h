//
// Created by jglrxavpok on 26/06/2023.
//

#pragma once

#include <Jolt/Renderer/DebugRenderer.h>
#include <engine/render/RenderContext.h>
#include <engine/render/resources/Pipeline.h>
#include <engine/render/resources/Vertex.h>
#include <engine/render/RenderPacket.h>

namespace Carrot::Render {
    class Viewport;
}

namespace Carrot::Physics {
    class JoltDebugRenderer: public ::JPH::DebugRenderer {
    public:
        JoltDebugRenderer(Carrot::Render::Viewport& debugViewport);

        // do the actual rendering
        void render(const Carrot::Render::Context& renderContext);

    public: // Jolt - record only
        void DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor) override;

        void DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor, ECastShadow inCastShadow = ECastShadow::Off) override;

        Batch CreateTriangleBatch(const Triangle *inTriangles, int inTriangleCount) override;

        Batch CreateTriangleBatch(const Vertex *inVertices, int inVertexCount, const JPH::uint32 *inIndices,
                                  int inIndexCount) override;

        void DrawGeometry(const JPH::Mat44& inModelMatrix, const JPH::AABox& inWorldSpaceBounds, float inLODScaleSq,
                          JPH::ColorArg inModelColor, const GeometryRef& inGeometry, ECullMode inCullMode,
                          ECastShadow inCastShadow, EDrawMode inDrawMode) override;

        void DrawText3D(JPH::RVec3Arg inPosition, const std::string_view& inString, JPH::ColorArg inColor,
                        float inHeight) override;

    private:
        std::vector<Carrot::Vertex> vertices; // every 3 vertex is a full triangle
        std::vector<std::uint32_t> indices; // every 3 index is a full triangle
        std::vector<Carrot::Render::Packet> batchDraws; // draws of Jolt DebugRenderer::Batch
        std::shared_ptr<Carrot::Pipeline> debugTrianglesPipeline;
        std::shared_ptr<Carrot::Pipeline> debugLinesPipeline;

        Carrot::Render::Viewport& debugViewport;
    };
} // Carrot::Physics
