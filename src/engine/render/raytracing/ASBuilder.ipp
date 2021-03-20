#include "ASBuilder.h"
#include "engine/render/resources/Buffer.h"
#include "engine/render/Model.h"
#include "engine/render/resources/Mesh.h"

// Logic based on NVIDIA nvpro-samples tutorial: https://nvpro-samples.github.io/vk_raytracing_tutorial_KHR/

template<typename VertexType>
vector<Carrot::GeometryInput*> Carrot::ASBuilder::addModelGeometries(const Carrot::Model& model) {
    vector<Carrot::GeometryInput*> geometries{};
    for(const auto& mesh : model.getMeshes()) {
        auto geom = addGeometries<VertexType>(mesh->getBackingBuffer(), mesh->getIndexCount(), 0, mesh->getBackingBuffer(), mesh->getVertexCount(), {mesh->getVertexStartOffset()});
        geometries.push_back(geom);
    }
    return geometries;
}

template<typename VertexType>
Carrot::GeometryInput* Carrot::ASBuilder::addGeometries(const Carrot::Buffer& indexBuffer, uint64_t indexCount, uint64_t indexOffset,
                                      const Carrot::Buffer& vertexBuffer, uint64_t vertexCount, const vector<uint64_t>& vertexOffsets) {
    // index buffer is assumed to be the same for all subranges
    // example usage: an original vertex buffer skinned by a compute shader, output to a global vertex buffer, but all using the same index buffer with offset
    auto& device = renderer.getLogicalDevice();
    auto indexAddress = device.getBufferAddress({.buffer = indexBuffer.getVulkanBuffer()});
    auto vertexAddress = device.getBufferAddress({.buffer = vertexBuffer.getVulkanBuffer()});
    registerIndexBuffer(indexBuffer, indexOffset, indexCount*sizeof(uint32_t));
    auto primitiveCount = indexCount / 3; // all triangles

    // create a GeometryInput for each vertex buffer sub-range
    bottomLevelGeometries.resize(bottomLevelGeometries.size()+1);
    GeometryInput& input = bottomLevelGeometries.back();
    for(uint64_t vertexOffset : vertexOffsets) {
        registerVertexBuffer(vertexBuffer, vertexOffset, vertexCount*sizeof(VertexType));

        input.geometries.emplace_back(vk::AccelerationStructureGeometryKHR {
            .geometryType = vk::GeometryTypeKHR::eTriangles,
            .geometry = vk::AccelerationStructureGeometryTrianglesDataKHR {
                    // vec3 triangle position, other attributes are passed via the vertex buffer used as a storage buffer (via descriptors)
                    .vertexFormat = vk::Format::eR32G32B32Sfloat,
                    .vertexData = vertexAddress + vertexOffset,
                    .vertexStride = sizeof(VertexType),
                    .maxVertex = static_cast<uint32_t>(vertexCount),
                    .indexType = vk::IndexType::eUint32,
                    .indexData = indexAddress + indexOffset,
                    .transformData = {},
            },
            .flags = vk::GeometryFlagBitsKHR::eOpaque, // TODO: opacity should be user-defined
        });

        // set correct range for sub vertex buffer
        input.buildRanges.emplace_back(vk::AccelerationStructureBuildRangeInfoKHR {
            .primitiveCount = static_cast<uint32_t>(primitiveCount),
            .primitiveOffset = 0,
            .firstVertex = 0,
            .transformOffset = 0,
        });
    }

    return &bottomLevelGeometries[bottomLevelGeometries.size()-1];
}
