//
// Created by jglrxavpok on 29/11/2020.
//

#pragma once
#include "engine/vulkan/includes.h"
#include "Vertex.h"
#include "engine/render/resources/Buffer.h"
#include "engine/Engine.h"

namespace Carrot {
    class Mesh: public DebugNameable {
    public:
        template<typename VertexType>
        explicit Mesh(Carrot::VulkanDriver& driver, const std::vector<VertexType>& vertices, const std::vector<uint32_t>& indices);

        void bind(const vk::CommandBuffer& buffer) const;
        /// Does not bind the original vertex buffer (but does bind the index buffer)
        void bindForIndirect(const vk::CommandBuffer& buffer) const;
        void draw(const vk::CommandBuffer& buffer, uint32_t instanceCount = 1) const;
        void indirectDraw(const vk::CommandBuffer& buffer, const Carrot::Buffer& indirectDraw, uint32_t drawCount) const;

        std::size_t getSizeOfSingleVertex() const;
        std::uint64_t getVertexCount() const;
        std::size_t getVertexSize() const;
        std::uint64_t getVertexStartOffset() const;
        Carrot::BufferView getVertexBuffer(); // TODO: make const

        std::uint64_t getIndexCount() const;
        std::size_t getIndexSize() const;
        std::uint64_t getIndexStartOffset() const;
        Carrot::BufferView getIndexBuffer(); // TODO: make const

        Carrot::Buffer& getBackingBuffer();
        const Carrot::Buffer& getBackingBuffer() const;

        std::uint64_t getMeshID() const;

    protected:
        void setDebugNames(const std::string& name) override;

    private:
        static uint64_t currentMeshID;

        Carrot::VulkanDriver& driver;
        /// <vertices>+<indices> at the end of the vertex list, to better use cache
        std::unique_ptr<Carrot::Buffer> vertexAndIndexBuffer = nullptr;

        /// How many indices are inside this mesh? Used for the actual drawcall
        std::uint64_t indexCount = 0;
        std::uint64_t vertexCount = 0;

        /// Index at which indices start inside the buffer
        std::uint64_t vertexStartOffset = 0;

        std::uint64_t meshID = 0;

        std::size_t sizeofVertex = 0;

    };
}

#include "Mesh.ipp"
