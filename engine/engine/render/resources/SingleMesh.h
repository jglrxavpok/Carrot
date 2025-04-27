//
// Created by jglrxavpok on 25/07/2022.
//

#pragma once

#include "Mesh.h"
#include "LightMesh.h"
#include "engine/vulkan/VulkanDriver.h"

namespace Carrot {
    /// Mesh that uploads the given vertices and indices to a single buffer.
    class SingleMesh: public Mesh {
    public:
        template<typename VertexType>
        explicit SingleMesh(const std::vector<VertexType>& vertices, const std::vector<uint32_t>& indices);
        ~SingleMesh();

        Carrot::Buffer& getBackingBuffer();
        const Carrot::Buffer& getBackingBuffer() const;

    public: // Mesh
        virtual std::size_t getSizeOfSingleVertex() const override;
        virtual std::uint64_t getVertexCount() const override;
        virtual std::size_t getVertexBufferSize() const override;
        virtual std::uint64_t getVertexStartOffset() const override;
        virtual Carrot::BufferView getVertexBuffer() override;
        virtual const Carrot::BufferView getVertexBuffer() const  override;

        virtual std::size_t getSizeOfSingleIndex() const override;
        virtual std::uint64_t getIndexCount() const override;
        virtual std::size_t getIndexBufferSize() const override;
        virtual std::uint64_t getIndexStartOffset() const override;
        virtual Carrot::BufferView getIndexBuffer() override;
        virtual const Carrot::BufferView getIndexBuffer() const override;

    public:
        LightMesh getSubMesh(std::size_t startVertex, std::size_t vertexCount, std::size_t startIndex, std::size_t indexCount) const;

    protected:
        void setDebugNames(const std::string& name) override;

    private:
        /// <vertices>+<indices> at the end of the vertex list, to better use cache
        std::unique_ptr<Carrot::Buffer> vertexAndIndexBuffer = nullptr;

        /// How many indices are inside this mesh? Used for the actual drawcall
        std::uint64_t indexCount = 0;
        std::uint64_t vertexCount = 0;

        /// Index at which indices start inside the buffer
        std::uint64_t vertexStartOffset = 0;

        std::size_t sizeofVertex = 0;
    };
}

#include "SingleMesh.ipp"