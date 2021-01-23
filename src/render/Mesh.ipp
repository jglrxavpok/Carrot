#include "Mesh.h"

template<typename VertexType>
Carrot::Mesh::Mesh(Carrot::Engine& engine, const vector<VertexType>& vertices, const vector<uint32_t>& indices): meshID(currentMeshID++), engine(engine) {
    const auto& queueFamilies = engine.getQueueFamilies();
    // create and allocate underlying buffer
    std::set<uint32_t> families = {
            queueFamilies.transferFamily.value(), queueFamilies.graphicsFamily.value()
    };

    vertexStartOffset = sizeof(uint32_t) * indices.size();
    if(vertexStartOffset % sizeof(uint32_t) != 0) {
        // align on uint32 boundary
        vertexStartOffset += sizeof(uint32_t) - (vertexStartOffset % sizeof(uint32_t));
    }
    indexCount = indices.size();
    vertexCount = vertices.size();
    vertexAndIndexBuffer = make_unique<Carrot::Buffer>(engine,
                                                       vertexStartOffset + sizeof(VertexType) * vertices.size(),
                                                       vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc /*TODO: tmp: used by skinning to copy to final buffer, instead of descriptor bind*/ | vk::BufferUsageFlagBits::eShaderDeviceAddress,
                                                       vk::MemoryPropertyFlagBits::eDeviceLocal,
                                                       families);

    // upload vertices
    vertexAndIndexBuffer->stageUploadWithOffsets(make_pair(vertexStartOffset, vertices), make_pair(static_cast<uint64_t>(0), indices));
}