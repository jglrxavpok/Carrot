//
// Created by jglrxavpok on 25/07/2022.
//

#include "SingleMesh.h"
#include "engine/Engine.h"
#include "engine/utils/Macros.h"
#include "engine/render/VulkanRenderer.h"

Carrot::SingleMesh::~SingleMesh() {
    GetRenderer().deferDestroy(getDebugName(), vertexAndIndexBuffer.release()); // BufferViews keeps a pointer to this buffer (for instance inside RenderPackets)
}

void Carrot::SingleMesh::setDebugNames(const std::string& name) {
    vertexAndIndexBuffer->setDebugNames(name);
}

std::uint64_t Carrot::SingleMesh::getIndexCount() const {
    return indexCount;
}

std::uint64_t Carrot::SingleMesh::getVertexBufferSize() const {
    return vertexCount * sizeofVertex;
}

std::size_t Carrot::SingleMesh::getSizeOfSingleVertex() const {
    return sizeofVertex;
}

std::size_t Carrot::SingleMesh::getSizeOfSingleIndex() const {
    return sizeof(std::uint32_t);
}

Carrot::BufferView Carrot::SingleMesh::getVertexBuffer() {
    return BufferView(nullptr, getBackingBuffer(), static_cast<vk::DeviceSize>(getVertexStartOffset()), static_cast<vk::DeviceSize>(getVertexBufferSize()));
}

Carrot::BufferView Carrot::SingleMesh::getIndexBuffer() {
    return BufferView(nullptr, getBackingBuffer(), static_cast<vk::DeviceSize>(getIndexStartOffset()), static_cast<vk::DeviceSize>(getIndexBufferSize()));
}

const Carrot::BufferView Carrot::SingleMesh::getVertexBuffer() const {
    return BufferView(nullptr, const_cast<Carrot::Buffer&>(getBackingBuffer()), static_cast<vk::DeviceSize>(getVertexStartOffset()), static_cast<vk::DeviceSize>(getVertexBufferSize()));
}

const Carrot::BufferView Carrot::SingleMesh::getIndexBuffer() const {
    return BufferView(nullptr, const_cast<Carrot::Buffer&>(getBackingBuffer()), static_cast<vk::DeviceSize>(getIndexStartOffset()), static_cast<vk::DeviceSize>(getIndexBufferSize()));
}

std::uint64_t Carrot::SingleMesh::getIndexBufferSize() const {
    return indexCount * sizeof(std::uint32_t);
}

std::uint64_t Carrot::SingleMesh::getVertexCount() const {
    return vertexCount;
}

std::uint64_t Carrot::SingleMesh::getVertexStartOffset() const {
    return vertexStartOffset;
}

std::uint64_t Carrot::SingleMesh::getIndexStartOffset() const {
    return 0;
}

Carrot::Buffer& Carrot::SingleMesh::getBackingBuffer() {
    return *vertexAndIndexBuffer;
}

const Carrot::Buffer& Carrot::SingleMesh::getBackingBuffer() const {
    return *vertexAndIndexBuffer;
}

Carrot::LightMesh Carrot::SingleMesh::getSubMesh(std::size_t startVertex, std::size_t vertexCount, std::size_t startIndex, std::size_t indexCount) const {
    Carrot::BufferView vertexBuffer = getVertexBuffer().subView(startVertex * sizeofVertex, vertexCount * sizeofVertex);
    Carrot::BufferView indexBuffer = getIndexBuffer().subView(startIndex * getSizeOfSingleIndex(), indexCount * getSizeOfSingleIndex());
    return LightMesh(vertexBuffer, indexBuffer, getSizeOfSingleVertex(), getSizeOfSingleIndex());
}