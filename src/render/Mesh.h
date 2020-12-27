//
// Created by jglrxavpok on 29/11/2020.
//

#pragma once
#include "vulkan/includes.h"
#include "render/Vertex.h"
#include "render/Buffer.h"
#include "Engine.h"

namespace Carrot {
    class Mesh: public DebugNameable {
    private:
        static uint64_t currentMeshID;

        Carrot::Engine& engine;
        /// <vertices>+<indices> at the end of the vertex list, to better use cache
        unique_ptr<Carrot::Buffer> vertexAndIndexBuffer = nullptr;

        /// How many indices are inside this mesh? Used for the actual drawcall
        uint64_t indexCount = 0;
        uint64_t vertexCount = 0;

        /// Index at which indices start inside the buffer
        uint64_t vertexStartOffset = 0;

        uint64_t meshID = 0;

    protected:
        void setDebugNames(const string& name) override;

    public:
        template<typename VertexType>
        explicit Mesh(Carrot::Engine& engine, const std::vector<VertexType>& vertices, const std::vector<uint32_t>& indices);

        void bind(const vk::CommandBuffer& buffer) const;
        /// Does not bind the original vertex buffer (but does bind the index buffer)
        void bindForIndirect(const vk::CommandBuffer& buffer) const;
        void draw(const vk::CommandBuffer& buffer, uint32_t instanceCount = 1) const;
        void indirectDraw(const vk::CommandBuffer& buffer, const Carrot::Buffer& indirectDraw, uint32_t drawCount) const;

        uint64_t getIndexCount() const;
        uint64_t getVertexCount() const;
        uint64_t getVertexStartOffset() const;

        Carrot::Buffer& getBackingBuffer();

        uint64_t getMeshID() const;
    };
}

#include "Mesh.ipp"
