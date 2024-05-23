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
         * @param indexBuffer must hold a multiple of 'indexSize' bytes, representing the indices of the mesh
         * @param vertexSize how many bytes does a single vertex take?
         * @param indexSize how many bytes does a single index take?
         */
        LightMesh(const Carrot::BufferView& vertexBuffer, const Carrot::BufferView& indexBuffer, std::size_t vertexSize, std::size_t indexSize);

        virtual std::size_t getSizeOfSingleIndex() const override;

        virtual size_t getSizeOfSingleVertex() const override;

        virtual uint64_t getVertexCount() const override;

        virtual size_t getVertexBufferSize() const override;

        virtual uint64_t getVertexStartOffset() const override;

        virtual BufferView getVertexBuffer() override;

        virtual const BufferView getVertexBuffer() const override;

        virtual uint64_t getIndexCount() const override;

        virtual size_t getIndexBufferSize() const override;

        virtual uint64_t getIndexStartOffset() const override;

        virtual BufferView getIndexBuffer() override;

        virtual const BufferView getIndexBuffer() const override;

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
