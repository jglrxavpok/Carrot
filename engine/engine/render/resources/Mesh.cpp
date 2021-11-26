//
// Created by jglrxavpok on 29/11/2020.
//

#include "Mesh.h"
#include "engine/render/resources/Buffer.h"

std::uint64_t Carrot::Mesh::currentMeshID = 0;

void Carrot::Mesh::bind(const vk::CommandBuffer& buffer) const {
    buffer.bindVertexBuffers(0, vertexAndIndexBuffer->getVulkanBuffer(), {vertexStartOffset});
    buffer.bindIndexBuffer(vertexAndIndexBuffer->getVulkanBuffer(), 0, vk::IndexType::eUint32);
}

void Carrot::Mesh::bindForIndirect(const vk::CommandBuffer& buffer) const {
    buffer.bindIndexBuffer(vertexAndIndexBuffer->getVulkanBuffer(), 0, vk::IndexType::eUint32);
}

void Carrot::Mesh::draw(const vk::CommandBuffer& buffer, std::uint32_t instanceCount) const {
    buffer.drawIndexed(indexCount, instanceCount, 0, 0, 0);
}

void Carrot::Mesh::indirectDraw(const vk::CommandBuffer& buffer, const Carrot::Buffer& indirectDraw, std::uint32_t drawCount) const {
    buffer.drawIndexedIndirect(indirectDraw.getVulkanBuffer(), 0, drawCount, sizeof(vk::DrawIndexedIndirectCommand));
}

void Carrot::Mesh::setDebugNames(const std::string& name) {
    nameSingle(driver, name, vertexAndIndexBuffer->getVulkanBuffer());
}

std::uint64_t Carrot::Mesh::getIndexCount() const {
    return indexCount;
}

std::uint64_t Carrot::Mesh::getVertexCount() const {
    return vertexCount;
}

std::uint64_t Carrot::Mesh::getMeshID() const {
    return meshID;
}

std::uint64_t Carrot::Mesh::getVertexStartOffset() const {
    return vertexStartOffset;
}

Carrot::Buffer& Carrot::Mesh::getBackingBuffer() {
    return *vertexAndIndexBuffer;
}
