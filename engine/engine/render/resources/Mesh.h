//
// Created by jglrxavpok on 29/11/2020.
//

#pragma once
#include "Vertex.h"
#include "engine/render/resources/Buffer.h"

namespace Carrot {
    class Mesh: public DebugNameable {
    public:
        using Ref = std::shared_ptr<Mesh>;

        Mesh();
        virtual ~Mesh() = default;

        void bind(const vk::CommandBuffer& buffer) const;
        /// Does not bind the original vertex buffer (but does bind the index buffer)
        void bindForIndirect(const vk::CommandBuffer& buffer) const;
        void draw(const vk::CommandBuffer& buffer, uint32_t instanceCount = 1) const;
        void indirectDraw(const vk::CommandBuffer& buffer, const Carrot::Buffer& indirectDraw, uint32_t drawCount) const;

        virtual std::size_t getSizeOfSingleVertex() const = 0;
        virtual std::uint64_t getVertexCount() const = 0;
        virtual std::size_t getVertexBufferSize() const = 0;
        virtual std::uint64_t getVertexStartOffset() const = 0;
        virtual Carrot::BufferView getVertexBuffer() = 0;
        virtual const Carrot::BufferView getVertexBuffer() const = 0;

        virtual std::uint64_t getIndexCount() const = 0;
        virtual std::size_t getIndexBufferSize() const = 0;
        virtual std::uint64_t getIndexStartOffset() const = 0;
        virtual Carrot::BufferView getIndexBuffer() = 0;
        virtual const Carrot::BufferView getIndexBuffer() const = 0;

        std::uint64_t getMeshID() const;

    private:
        static std::atomic<std::uint64_t> currentMeshID;

        std::uint64_t meshID = 0;
    };
}
