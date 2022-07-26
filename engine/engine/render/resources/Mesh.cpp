//
// Created by jglrxavpok on 29/11/2020.
//

#include "Mesh.h"
#include "engine/render/resources/Buffer.h"

std::atomic<std::uint64_t> Carrot::Mesh::currentMeshID = 0;

Carrot::Mesh::Mesh(): meshID(++currentMeshID) {}

void Carrot::Mesh::bind(const vk::CommandBuffer& buffer) const {
    buffer.bindVertexBuffers(0, getVertexBuffer().getVulkanBuffer(), {getVertexBuffer().getStart()});
    buffer.bindIndexBuffer(getIndexBuffer().getVulkanBuffer(), getIndexBuffer().getStart(), vk::IndexType::eUint32);
}

void Carrot::Mesh::bindForIndirect(const vk::CommandBuffer& buffer) const {
    buffer.bindIndexBuffer(getIndexBuffer().getVulkanBuffer(), 0, vk::IndexType::eUint32);
}

void Carrot::Mesh::draw(const vk::CommandBuffer& buffer, std::uint32_t instanceCount) const {
    buffer.drawIndexed(getIndexCount(), instanceCount, 0, 0, 0);
}

void Carrot::Mesh::indirectDraw(const vk::CommandBuffer& buffer, const Carrot::Buffer& indirectDraw, std::uint32_t drawCount) const {
    buffer.drawIndexedIndirect(indirectDraw.getVulkanBuffer(), 0, drawCount, sizeof(vk::DrawIndexedIndirectCommand));
}

std::uint64_t Carrot::Mesh::getMeshID() const {
    return meshID;
}
