//
// Created by jglrxavpok on 26/07/2022.
//

#include "LightMesh.h"

namespace Carrot {
    LightMesh::LightMesh(const BufferView& vertexBuffer, const BufferView& indexBuffer, std::size_t vertexSize): vertexBuffer(vertexBuffer), indexBuffer(indexBuffer), vertexSize(vertexSize) {
        verify(vertexSize > 0, "vertexSize must be > 0");
        verify(vertexBuffer.getSize() > 0, "vertex buffer must not be empty");
        verify(indexBuffer.getSize() > 0, "index buffer must not be empty");
        verify(vertexBuffer.getSize() % vertexSize == 0, "Vertex buffer must hold a multiple of 'vertexSize' bytes");
        verify(indexBuffer.getSize() % indexSize == 0, "Index buffer must hold a multiple of 4 (size of u32)");

        vertexCount = vertexBuffer.getSize() / vertexSize;
        indexCount = indexBuffer.getSize() / indexSize;
    }

    size_t LightMesh::getSizeOfSingleVertex() const {
        return vertexSize;
    }

    uint64_t LightMesh::getVertexCount() const {
        return vertexCount;
    }

    size_t LightMesh::getVertexBufferSize() const {
        return vertexBuffer.getSize();
    }

    uint64_t LightMesh::getVertexStartOffset() const {
        return vertexBuffer.getStart();
    }

    BufferView LightMesh::getVertexBuffer() {
        return vertexBuffer;
    }

    const BufferView LightMesh::getVertexBuffer() const {
        return vertexBuffer;
    }

    uint64_t LightMesh::getIndexCount() const {
        return indexCount;
    }

    size_t LightMesh::getIndexBufferSize() const {
        return indexBuffer.getSize();
    }

    uint64_t LightMesh::getIndexStartOffset() const {
        return indexBuffer.getStart();
    }

    BufferView LightMesh::getIndexBuffer() {
        return indexBuffer;
    }

    const BufferView LightMesh::getIndexBuffer() const {
        return indexBuffer;
    }

    void LightMesh::setDebugNames(const std::string& name) {
        vertexBuffer.getBuffer().setDebugNames(name + " - Vertex Buffer");
        indexBuffer.getBuffer().setDebugNames(name + " - Index Buffer");
    }

}