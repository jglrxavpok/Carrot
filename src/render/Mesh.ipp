#include "Mesh.h"

template<typename VertexType>
Carrot::Mesh::Mesh(Carrot::Engine& engine, const vector<VertexType>& vertices, const vector<uint32_t>& indices): engine(engine) {
    const auto& queueFamilies = engine.getQueueFamilies();
    // create and allocate underlying buffer
    std::set<uint32_t> families = {
            queueFamilies.transferFamily.value(), queueFamilies.graphicsFamily.value()
    };

    indexStartOffset = sizeof(VertexType) * vertices.size();
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