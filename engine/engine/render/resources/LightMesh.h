//
// Created by jglrxavpok on 26/07/2022.
//

#pragma once

#include "Mesh.h"

namespace Carrot {
    /// Mesh that references pre-existing vertex & index buffers. That's it.
    class LightMesh: public Mesh {
    public:
        /**
         * Creates the mesh
         * @param vertexBuffer must hold a multiple of 'vertexSize' bytes, representing the vertices of the mesh
         * @param indexBuffer must hold u32 values, representing the indices of the mesh
         * @param vertexSize how many bytes does a single vertex take?
         */
        LightMesh(const Carrot::BufferView& vertexBuffer, const Carrot::BufferView& indexBuffer, std::size_t vertexSize);

        size_t getSizeOfSingleVertex() const override;

        uint64_t getVertexCount() const override;

        size_t getVertexBufferSize() const override;

        uint64_t getVertexStartOffset() const override;

        BufferView getVertexBuffer() override;

        const BufferView getVertexBuffer() const override;

        uint64_t getIndexCount() const override;

        size_t getIndexBufferSize() const override;

        uint64_t getIndexStartOffset() const override;

        BufferView getIndexBuffer() override;

        const BufferView getIndexBuffer() const override;

    protected:
        void setDebugNames(const std::string& name) override;

    private:
        Carrot::BufferView vertexBuffer;
        Carrot::BufferView indexBuffer;
        std::size_t vertexSize = 0;
        std::size_t indexSize = sizeof(std::uint32_t);
        std::size_t vertexCount = 0; // pre-computed in constructor
        std::size_t indexCount = 0; // pre-computed in constructor
    };

}
