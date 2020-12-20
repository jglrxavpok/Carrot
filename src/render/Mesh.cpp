//
// Created by jglrxavpok on 29/11/2020.
//

#include "Mesh.h"
#include "render/Buffer.h"

void Carrot::Mesh::bind(const vk::CommandBuffer& buffer) const {
    buffer.bindVertexBuffers(0, vertexAndIndexBuffer->getVulkanBuffer(), {0});
    buffer.bindIndexBuffer(vertexAndIndexBuffer->getVulkanBuffer(), indexStartOffset, vk::IndexType::eUint32);
}

void Carrot::Mesh::draw(const vk::CommandBuffer& buffer, uint32_t instanceCount) const {
    buffer.drawIndexed(indexCount, instanceCount, 0, 0, 0);
}
