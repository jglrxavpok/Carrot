//
// Created by jglrxavpok on 29/11/2020.
//

#include "Mesh.h"
#include "render/Buffer.h"

Carrot::Mesh::Mesh(Carrot::Engine& engine, const vector<Carrot::Vertex>& vertices, const vector<uint32_t>& indices): engine(engine) {
    const auto& queueFamilies = engine.getQueueFamilies();
    // create and allocate underlying buffer
    std::set<uint32_t> families = {
            queueFamilies.transferFamily.value(), queueFamilies.graphicsFamily.value()
    };

    indexStartOffset = sizeof(Carrot::Vertex) * vertices.size();
    if(indexStartOffset % sizeof(uint32_t) != 0) {
        // align on uint32 boundary
        indexStartOffset += sizeof(uint32_t) - (indexStartOffset % sizeof(uint32_t));
    }
    indexCount = indices.size();
    vertexAndIndexBuffer = make_unique<Carrot::Buffer>(engine,
                                                       indexStartOffset + sizeof(uint32_t) * indices.size(),
                                                        vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
                                                        vk::MemoryPropertyFlagBits::eDeviceLocal,
                                                        families);

    // upload vertices
    vertexAndIndexBuffer->stageUploadWithOffsets(make_pair(static_cast<uint64_t>(0), vertices), make_pair(indexStartOffset, indices));
}

void Carrot::Mesh::bind(const vk::CommandBuffer& buffer) const {
    buffer.bindVertexBuffers(0, vertexAndIndexBuffer->getVulkanBuffer(), {0});
    buffer.bindIndexBuffer(vertexAndIndexBuffer->getVulkanBuffer(), indexStartOffset, vk::IndexType::eUint32);
}

void Carrot::Mesh::draw(const vk::CommandBuffer& buffer, uint32_t instanceCount) const {
    buffer.drawIndexed(indexCount, instanceCount, 0, 0, 0);
}
