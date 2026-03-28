//
// Created by jglrxavpok on 05/11/2022.
//

#include <models/ModelProcessing.h>
#include <core/utils/CarrotTinyGLTF.h>
#include <core/utils/UserNotifications.h>
#include <core/scene/LoadedScene.h>
#include <core/Macros.h>
#include <core/scene/GLTFLoader.h>
#include <models/GLTFWriter.h>
#include <unordered_set>
#include <core/io/Logging.hpp>
#include <glm/gtx/component_wise.hpp>
#include <core/tasks/Tasks.h>
#include <core/data/Hashes.h>
#include <core/containers/KDTree.hpp>
#include <glm/gtx/hash.hpp>

#include <models/MikkTSpaceInterface.h>

#define CLUSTERLOD_IMPLEMENTATION
#include <meshoptimizer.h>
#include <clusterlod.h>

#define IDXTYPEWIDTH 64
#define REALTYPEWIDTH 64
#include <metis.h>
#include <robin_hood.h>
#include <core/math/Sphere.h>
#include <core/render/VkAccelerationStructureHeader.h>
#include <core/scene/AssimpLoader.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>
#include <gpu_assistance/VulkanHelper.h>

#include "assimp/Importer.hpp"

namespace Fertilizer {
    static constexpr std::size_t MaxVertices = 64;
    static constexpr std::size_t MaxTriangles = 128;

    constexpr float Epsilon = 10e-16;
    constexpr const char* const KHR_TEXTURE_BASISU_EXTENSION_NAME = "KHR_texture_basisu";

    using fspath = std::filesystem::path;
    using namespace Carrot::IO;
    using namespace Carrot::Render;

    bool gltfReadWholeFile(std::vector<unsigned char>* out,
                           std::string* err, const std::string& filepath,
                           void* userData) {
        try {
            Carrot::IO::FileHandle f { filepath, OpenMode::Read };

            std::size_t size = f.getSize();

            out->resize(size);
            f.read(out->data(), out->size(), 0);
        } catch (std::filesystem::filesystem_error& e) {
            *err = e.what();
            return false;
        }
        return true;
    }

    std::string gltfExpandFilePath(const std::string& filepath, void* userData) {
        const fspath& basePath = *reinterpret_cast<fspath*>(userData);
        return (basePath / filepath).string();
    }

    bool gltfFileExists(const std::string& abs_filename, void* userData) {
        return std::filesystem::exists(abs_filename);
    }

    /**
     * "Expands" the vertex buffer: this is the exact opposite of indexing, we separate vertex info for each face
     *  otherwise keeping the same index buffer will provide incorrect results after attribute generation
     */
    static ExpandedMesh expandMesh(const LoadedPrimitive& primitive, const Carrot::NotificationID& notifID) {
        Carrot::UserNotifications::getInstance().setBody(notifID, Carrot::sprintf("Expand mesh %s", primitive.name.c_str()));

        ExpandedMesh expanded;
        const bool isSkinned = primitive.isSkinned;
        const std::size_t vertexCount = isSkinned ? primitive.skinnedVertices.size() : primitive.vertices.size();
        const std::size_t indexCount = primitive.indices.size();
        expanded.vertices.resize(indexCount);
        expanded.duplicatedVertices.resize(vertexCount);
        for(std::size_t i = 0; i < indexCount; i++) {
            const std::uint32_t index = primitive.indices[i];
            if(isSkinned) {
                expanded.vertices[i].vertex = primitive.skinnedVertices[index];
            } else {
                expanded.vertices[i].vertex.pos = primitive.vertices[index].pos;
                expanded.vertices[i].vertex.normal = primitive.vertices[index].normal;
                expanded.vertices[i].vertex.tangent = primitive.vertices[index].tangent;
                expanded.vertices[i].vertex.color = primitive.vertices[index].color;
                expanded.vertices[i].vertex.uv = primitive.vertices[index].uv;
            }
            expanded.vertices[i].originalIndex = index;

            expanded.duplicatedVertices[index].push_back(i);
        }

        return std::move(expanded);
    }

    // struct used to merge vertices based on similarity
    struct VertexKey {
        glm::vec3 pos;
        glm::vec2 uv;
        glm::vec3 color;
        glm::vec3 normal;
        glm::i8vec4 boneIDs;
        glm::vec4 boneWeights;

        bool operator==(const VertexKey&) const = default;
    };

    struct VertexKeyHasher {
        std::size_t operator()(const VertexKey& k) const {
            std::size_t h = std::hash<glm::vec3>{}(k.pos);
            Carrot::hash_combine(h, std::hash<glm::vec2>{}(k.uv));
            Carrot::hash_combine(h, std::hash<glm::vec3>{}(k.color));
            Carrot::hash_combine(h, std::hash<glm::vec3>{}(k.normal));
            Carrot::hash_combine(h, std::hash<glm::i8vec4>{}(k.boneIDs));
            Carrot::hash_combine(h, std::hash<glm::vec4>{}(k.boneWeights));
            return h;
        }
    };

    /**
     * Generate indexed mesh into 'out' from a non-indexed mesh inside ExpandedMesh
     */
    static void collapseMesh(LoadedPrimitive& out, ExpandedMesh& mesh, const Carrot::NotificationID& notifID) {
        Carrot::UserNotifications::getInstance().setBody(notifID, Carrot::sprintf("Collapse mesh %s", out.name.c_str()));

        out.vertices.clear();
        out.skinnedVertices.clear();
        out.indices.clear();
        std::uint32_t nextIndex = 0;
        const bool isSkinned = out.isSkinned;

        struct DeduplicatedVertex {
            std::size_t index = 0; // index this vertex will have inside vertex buffer
            std::size_t count = 0; // how many vertices have been accumulated in this vertex?
            Carrot::SkinnedVertex vertex;
        };

        std::unordered_map<VertexKey, DeduplicatedVertex, VertexKeyHasher> deduplicatedVertices;

        std::size_t maxVertexIndex = 0;
        for(std::size_t vertexIndex = 0; vertexIndex < mesh.vertices.size(); vertexIndex++) {
            auto& duplicatedVertex = mesh.vertices[vertexIndex];
            verify(!duplicatedVertex.newIndex.has_value(), "Programming error: duplicated vertex must not already have an index in the new mesh");

            VertexKey key {
                .pos = duplicatedVertex.vertex.pos.xyz(),
                .uv = duplicatedVertex.vertex.uv,
                .color = duplicatedVertex.vertex.color,
                .normal = duplicatedVertex.vertex.normal,
                .boneIDs = duplicatedVertex.vertex.boneIDs,
                .boneWeights = duplicatedVertex.vertex.boneWeights,
            };

            auto [iter, bWasNew] = deduplicatedVertices.try_emplace(key);
            DeduplicatedVertex& deduplicatedVertex = iter->second;
            if(bWasNew) {
                deduplicatedVertex.vertex = duplicatedVertex.vertex;
                if(duplicatedVertex.vertex.tangent.w < 0.0f) {
                    deduplicatedVertex.vertex.tangent *= -1;
                    deduplicatedVertex.vertex.tangent.w = 1.0f;
                }
                deduplicatedVertex.index = maxVertexIndex++;
            } else {
                deduplicatedVertex.vertex.normal += duplicatedVertex.vertex.normal;

                // ensure W does not become 0
                if(duplicatedVertex.vertex.tangent.w < 0.0f) {
                    duplicatedVertex.vertex.tangent += -duplicatedVertex.vertex.tangent;
                } else {
                    deduplicatedVertex.vertex.tangent += duplicatedVertex.vertex.tangent;
                }
            }
            deduplicatedVertex.count++;
            out.indices.emplace_back(iter->second.index);
        }

        verify(maxVertexIndex == deduplicatedVertices.size(), "Mismatch between maximum vertex index given to a vertex, and total count of vertices");
        if(isSkinned) {
            out.skinnedVertices.resize(maxVertexIndex);
            for(const auto& [_, deduplicatedVertex] : deduplicatedVertices) {
                auto v = deduplicatedVertex.vertex;
                v.normal /= deduplicatedVertex.count;
                v.tangent /= deduplicatedVertex.count;
                out.skinnedVertices[deduplicatedVertex.index] = v;
            }
        } else {
            out.vertices.resize(maxVertexIndex);
            for(const auto& [_, deduplicatedVertex] : deduplicatedVertices) {
                auto v = deduplicatedVertex.vertex;
                v.normal /= deduplicatedVertex.count;
                v.tangent /= deduplicatedVertex.count;
                out.vertices[deduplicatedVertex.index] = v; // NOLINT(*-slicing): slicing is on purpose
            }
        }
    }

    static void generateFlatNormals(ExpandedMesh& mesh, const Carrot::NotificationID& notifID) {
        Carrot::UserNotifications::getInstance().setBody(notifID, "Generate normals");

        std::size_t faceCount = mesh.vertices.size() / 3;
        for(std::size_t face = 0; face < faceCount; face++) {
            Carrot::Vertex& a = mesh.vertices[face * 3 + 0].vertex;
            Carrot::Vertex& b = mesh.vertices[face * 3 + 1].vertex;
            Carrot::Vertex& c = mesh.vertices[face * 3 + 2].vertex;

            glm::vec3 ab = (b.pos - a.pos).xyz();
            glm::vec3 bc = (c.pos - b.pos).xyz();
            glm::vec3 ac = (c.pos - a.pos).xyz();

            if(glm::dot(ab, ab) <= Epsilon
            || glm::dot(bc, bc) <= Epsilon
            || glm::dot(ac, ac) <= Epsilon
            ) {
                Carrot::Log::warn("Degenerate triangle (face = %llu)", face);
            }

            a.normal = glm::normalize(glm::cross(ab, ac));
            b.normal = glm::normalize(glm::cross(bc, -ab));
            c.normal = glm::normalize(glm::cross(-ac, -bc));
        }
    }

    static void generateMikkTSpaceTangents(ExpandedMesh& mesh, const Carrot::NotificationID& notifID) {
        Carrot::UserNotifications::getInstance().setBody(notifID, "Generate tangents");

        bool r = generateTangents(mesh);
        if(!r) {
            Carrot::Log::error("Could not generate tangents for mesh");
        }
    }

    /**
     * If tangents are colinear with normals, make tangent follow an edge of the triangle. This case can happen when
     * applying Mikkt-Space with no UV mapping (either inside 'generateMikkTSpaceTangents' or other tools, eg Blender)
     * @param mesh the mesh to clean up
     */
    static void cleanupTangents(ExpandedMesh& mesh, const Carrot::NotificationID& notifID) {
        Carrot::UserNotifications::getInstance().setBody(notifID, "Clean tangents");

        verify(mesh.vertices.size() % 3 == 0, "Only triangle meshs are supported");
        for (std::size_t i = 0; i < mesh.vertices.size(); i += 3) {
            auto isCloseToCollinear = [](const glm::vec3& a, const glm::vec3& b) {
                constexpr float epsilon = 10e-12f;
                const glm::vec3 rejected = b - glm::dot(a, b) * b;
                return glm::all(glm::lessThan(glm::abs(rejected), glm::vec3(epsilon)));
            };

            bool needsRegeneration = false;
            for(std::size_t j = 0; j < 3; j++) {
                const glm::vec3& normal = mesh.vertices[i + j].vertex.normal;
                const glm::vec3 tangent = mesh.vertices[i + j].vertex.tangent.xyz();
                needsRegeneration |= isCloseToCollinear(normal, tangent);
            }

            if(needsRegeneration) {
                const glm::vec3 edge = mesh.vertices[i + 1].vertex.pos.xyz() - mesh.vertices[i + 0].vertex.pos.xyz();
                for (std::size_t j = 0; j < 3; j++) {
                    glm::vec4& tangentW = mesh.vertices[i + j].vertex.tangent;
                    tangentW = glm::vec4(glm::normalize(edge), 1.0f); // W=1.0f but no thought was put behind this value
                }
            }
        }
    }

    struct MeshletGroup {
        std::vector<std::size_t> meshlets;
    };

    /**
     * From a primitive with meshlets, pregenerate BLASes for the groups of meshlets inside the primitive
     * Note: these BLASes may be valid only for a given driver version :c
     */
    static void prebuildGroupBLASes(VulkanHelper& vkHelper, LoadedScene& scene, NodeKey nodeKey, std::size_t primitiveIndex, const glm::mat4& instanceTransform) {
        LoadedPrimitive& primitive = scene.primitives[primitiveIndex];
        std::uint32_t groupCount = 0;
        for(const auto& meshlet : primitive.meshlets) {
            groupCount = std::max(groupCount, meshlet.groupIndex+1);
        }

        Carrot::Vector<MeshletGroup> rebuiltGroups;
        rebuiltGroups.resize(groupCount);
        for(std::size_t i = 0; i < primitive.meshlets.size(); i++) {
            const Meshlet& meshlet = primitive.meshlets[i];
            rebuiltGroups[meshlet.groupIndex].meshlets.push_back(i);
        }

        // maybe this could be done directly inside generateClusterHierarchy, but speed is not the most important thing for the asset pipeline
        // (but needs to stay reasonable)
        Carrot::StackAllocator allocator { Carrot::Allocator::getDefault() };
        for(std::size_t groupIndex = 0; groupIndex < groupCount; groupIndex++) {
            const MeshletGroup& group = rebuiltGroups[groupIndex];
            allocator.clear();

            if(group.meshlets.empty()) {
                continue;
            }

            // generate the meshes that will be used to generate the BLAS
            // (that is: the concatenation of the meshlets composing the group)
            // the runtime expects one mesh per cluster, otherwise the lighting shader may access invalid memory

            Carrot::Vector<glm::vec3> verticesCPU{ allocator };
            Carrot::Vector<std::uint16_t> indicesCPU{ allocator };
            Carrot::Vector<std::size_t> firstVertices { allocator };
            Carrot::Vector<std::size_t> firstIndices { allocator };
            firstVertices.resize(group.meshlets.size());
            firstIndices.resize(group.meshlets.size());
            for(std::size_t groupMeshletIndex = 0; groupMeshletIndex < group.meshlets.size(); groupMeshletIndex++) {
                const auto& meshletIndex = group.meshlets[groupMeshletIndex];
                const auto& meshlet = primitive.meshlets[meshletIndex];

                const std::size_t firstVertexIndex = verticesCPU.size();
                firstVertices[groupMeshletIndex] = firstVertexIndex;
                verticesCPU.resize(firstVertexIndex + meshlet.vertexCount);

                const std::size_t firstIndexIndex = indicesCPU.size();
                firstIndices[groupMeshletIndex] = firstIndexIndex;
                indicesCPU.resize(firstIndexIndex + meshlet.indexCount);

                for(std::size_t index = 0; index < meshlet.vertexCount; index++) {
                    verticesCPU[index + firstVertexIndex] = glm::vec3 { primitive.vertices[primitive.meshletVertexIndices[index + meshlet.vertexOffset]].pos.xyz() };
                }
                for(std::size_t index = 0; index < meshlet.indexCount; index++) {
                    indicesCPU[index + firstIndexIndex] = static_cast<std::uint16_t>(primitive.meshletIndices[index + meshlet.indexOffset]);
                }
            }

            GPUBuffer vertices = vkHelper.newHostVisibleBuffer(verticesCPU.size() * sizeof(glm::vec3), vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR);
            GPUBuffer indices = vkHelper.newHostVisibleBuffer(indicesCPU.size() * sizeof(std::uint16_t), vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR);
            void* pGPUVertices = vkHelper.getDevice().mapMemory(*vertices.vkMemory, 0, VK_WHOLE_SIZE, {}, vkHelper.getDispatcher());
            memcpy(pGPUVertices, verticesCPU.data(), verticesCPU.size() * sizeof(glm::vec3));
            void* pGPUIndices = vkHelper.getDevice().mapMemory(*indices.vkMemory, 0, VK_WHOLE_SIZE, {}, vkHelper.getDispatcher());
            memcpy(pGPUIndices, indicesCPU.data(), indicesCPU.size() * sizeof(std::uint16_t));
            vk::DeviceAddress verticesAddress = vkHelper.getDevice().getBufferAddress(vk::BufferDeviceAddressInfo { .buffer = *vertices.vkBuffer }, vkHelper.getDispatcher());
            vk::DeviceAddress indicesAddress = vkHelper.getDevice().getBufferAddress(vk::BufferDeviceAddressInfo { .buffer = *indices.vkBuffer }, vkHelper.getDispatcher());

            // compute storage size of BLAS
            GPUBuffer rtTransform = vkHelper.newHostVisibleBuffer(sizeof(vk::TransformMatrixKHR), vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR);
            vk::TransformMatrixKHR* pRTTransform = static_cast<vk::TransformMatrixKHR *>(vkHelper.getDevice().mapMemory(*rtTransform.vkMemory, 0, VK_WHOLE_SIZE, {}, vkHelper.getDispatcher()));
            *pRTTransform = VulkanHelper::glmToRTTransformMatrix(instanceTransform);
            vk::DeviceAddress rtTransformAddress = vkHelper.getDevice().getBufferAddress(vk::BufferDeviceAddressInfo { .buffer = *rtTransform.vkBuffer }, vkHelper.getDispatcher());

            Carrot::Vector<vk::AccelerationStructureGeometryKHR> geometries { allocator };
            Carrot::Vector<std::uint32_t> maxPrimitiveCounts { allocator };
            Carrot::Vector<vk::AccelerationStructureBuildRangeInfoKHR> buildRanges { allocator };
            geometries.resize(group.meshlets.size());
            maxPrimitiveCounts.resize(group.meshlets.size());
            buildRanges.resize(group.meshlets.size());
            for(std::size_t groupMeshletIndex = 0; groupMeshletIndex < group.meshlets.size(); groupMeshletIndex++) {
                const auto& meshletIndex = group.meshlets[groupMeshletIndex];
                const auto& meshlet = primitive.meshlets[meshletIndex];
                vk::AccelerationStructureGeometryKHR& geometry = geometries[groupMeshletIndex];
                geometry.geometryType = vk::GeometryTypeKHR::eTriangles;
                geometry.flags = vk::GeometryFlagBitsKHR::eOpaque;

                geometry.geometry = vk::AccelerationStructureGeometryTrianglesDataKHR {
                    .vertexFormat = vk::Format::eR32G32B32Sfloat,
                    .vertexData = verticesAddress + sizeof(glm::vec3) * firstVertices[groupMeshletIndex],
                    .vertexStride = sizeof(glm::vec3),
                    .maxVertex = meshlet.vertexCount-1,
                    .indexType = vk::IndexType::eUint16,
                    .indexData = indicesAddress + sizeof(std::uint16_t) * firstIndices[groupMeshletIndex],
                    .transformData = rtTransformAddress,
                };

                maxPrimitiveCounts[groupMeshletIndex] = meshlet.indexCount / 3;
                buildRanges[groupMeshletIndex].primitiveCount = maxPrimitiveCounts[groupMeshletIndex];
            }

            vk::AccelerationStructureBuildGeometryInfoKHR buildInfo {
                .type = vk::AccelerationStructureTypeKHR::eBottomLevel,
                .flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace,
                .mode = vk::BuildAccelerationStructureModeKHR::eBuild,
                .geometryCount = static_cast<std::uint32_t>(geometries.size()),
                .pGeometries = geometries.data(),
            };

            vk::AccelerationStructureBuildSizesInfoKHR sizeInfo = vkHelper.getDevice().getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eHostOrDevice, buildInfo, maxPrimitiveCounts, vkHelper.getDispatcher());

            GPUBuffer scratchBuffer = vkHelper.newDeviceLocalBuffer(sizeInfo.buildScratchSize, vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eStorageBuffer);
            vk::DeviceAddress scratchBufferAddress = vkHelper.getDevice().getBufferAddress(vk::BufferDeviceAddressInfo {
                .buffer = *scratchBuffer.vkBuffer,
            }, vkHelper.getDispatcher());

            // create and build BLAS
            vk::DeviceSize asSize = sizeInfo.accelerationStructureSize;
            GPUBuffer asBuffer = vkHelper.newHostVisibleBuffer(asSize, vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR);
            vk::AccelerationStructureCreateInfoKHR createInfo {
                .buffer = *asBuffer.vkBuffer,
                .size = asSize,
                .type = vk::AccelerationStructureTypeKHR::eBottomLevel,
            };
            auto as = vkHelper.getDevice().createAccelerationStructureKHRUnique(createInfo, nullptr, vkHelper.getDispatcher());

            buildInfo.scratchData = scratchBufferAddress;
            buildInfo.dstAccelerationStructure = *as;

            auto queryPool = vkHelper.getDevice().createQueryPoolUnique(vk::QueryPoolCreateInfo {
                .queryType = vk::QueryType::eAccelerationStructureSerializationSizeKHR,
                .queryCount = 1,
            }, nullptr, vkHelper.getDispatcher());
            vkHelper.getDevice().resetQueryPool(*queryPool, 0, 1, vkHelper.getDispatcher());
            vkHelper.executeCommands([&](vk::CommandBuffer cmds) {
                cmds.buildAccelerationStructuresKHR(buildInfo, buildRanges.cdata(), vkHelper.getDispatcher());
            });
            vkHelper.executeCommands([&](vk::CommandBuffer cmds) {
                cmds.writeAccelerationStructuresPropertiesKHR(1, &as.get(), vk::QueryType::eAccelerationStructureSerializationSizeKHR, *queryPool, 0, vkHelper.getDispatcher());
            });

            auto queryResult = vkHelper.getDevice().getQueryPoolResult<std::uint64_t>(*queryPool, 0, 1, sizeof(std::uint64_t), vk::QueryResultFlagBits::eWait | vk::QueryResultFlagBits::e64, vkHelper.getDispatcher());
            verify(queryResult.result == vk::Result::eSuccess, "Failed to get query result");
            const std::uint64_t serializedSize = queryResult.value;

            // copy BLAS to cpu buffer
            GPUBuffer serializedASStorageBuffer = vkHelper.newHostVisibleBuffer(serializedSize, vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress);
            vk::DeviceAddress serializedASStorageBufferAddress = vkHelper.getDevice().getBufferAddress(vk::BufferDeviceAddressInfo {
                .buffer = *serializedASStorageBuffer.vkBuffer,
            }, vkHelper.getDispatcher());
            vk::CopyAccelerationStructureToMemoryInfoKHR copyOp {
                .src = *as,
                .dst = serializedASStorageBufferAddress,
                .mode = vk::CopyAccelerationStructureModeKHR::eSerialize,
            };
            vkHelper.executeCommands([&](vk::CommandBuffer cmds) {
                cmds.copyAccelerationStructureToMemoryKHR(copyOp, vkHelper.getDispatcher());
            });

            void* serializedASPtr = vkHelper.getDevice().mapMemory(*serializedASStorageBuffer.vkMemory, 0, VK_WHOLE_SIZE, {}, vkHelper.getDispatcher());
            // TODO: compress blas bytes?

            // copy cpu buffer to storage
            // key = nodeIndex + meshIndex + groupIndex
            PrecomputedBLAS& precomputedBLAS = scene.precomputedBLASes[nodeKey][Carrot::Pair(static_cast<std::uint32_t>(primitiveIndex), static_cast<std::uint32_t>(groupIndex))];
            precomputedBLAS.blasBytes.resize(serializedSize);
            memcpy(precomputedBLAS.blasBytes.data(), serializedASPtr, precomputedBLAS.blasBytes.size());
        }
    }

    /**
     * From this primitive's vertex & index buffer, generate meshlets/clusters
     */
    static void generateClusterHierarchy(LoadedPrimitive& primitive, float simplifyScale) {
        Carrot::NotificationID notificationID = Carrot::UserNotifications::getInstance().showNotification(Carrot::NotificationDescription {
            .title = Carrot::sprintf("Generating LODs of %s", primitive.name.c_str()),
        });
        CLEANUP(Carrot::UserNotifications::getInstance().closeNotification(notificationID));
        clodConfig config = clodDefaultConfigRT(MaxTriangles);
        config.max_vertices = MaxVertices;
        clodMesh mesh{};

        mesh.indices = primitive.indices.data();
        mesh.index_count = primitive.indices.size();
        mesh.vertex_count = primitive.vertices.size();
        mesh.vertex_positions = &primitive.vertices[0].pos.x; // assumes pos x is first element
        mesh.vertex_positions_stride = sizeof(Carrot::Vertex);
        mesh.vertex_attributes = &primitive.vertices[0].color.x;
        mesh.vertex_attributes_stride = sizeof(Carrot::Vertex);

        mesh.attribute_count = 12; // float count, with padding
        float attributeWeights[12];
        for (i32 i = 0; i < mesh.attribute_count; i++) {
            attributeWeights[i] = 0.5f;
        }
        mesh.attribute_weights = attributeWeights;
        mesh.attribute_protect_mask = (1<<12) | (1<<13); // protect UV

        i32 groupIndex = 0;

        Carrot::Vector<clodBounds> groupBounds;

        clodBuild(config, mesh, [&](clodGroup group, const clodCluster* clusters, size_t clusterCount) -> int {
            const i32 currentGroupIndex = groupIndex++;

            if (groupBounds.size() <= currentGroupIndex) {
                groupBounds.resize(groupIndex);
            }

            groupBounds[currentGroupIndex] = group.simplified;

            i32 additionalMeshletIndexCount = 0;
            i32 additionalMeshletVertexCount = 0;
            for (i32 clusterIndex = 0; clusterIndex < clusterCount; clusterIndex++) {
                additionalMeshletIndexCount += clusters[clusterIndex].index_count;
                additionalMeshletVertexCount += clusters[clusterIndex].vertex_count;
            }
            i32 meshletIndexOffset = primitive.meshletIndices.size();
            i32 meshletVertexOffset = primitive.meshletVertexIndices.size();
            i32 meshletsOffset = primitive.meshlets.size();
            primitive.meshletIndices.resize(meshletIndexOffset + additionalMeshletIndexCount);
            primitive.meshletVertexIndices.resize(meshletVertexOffset + additionalMeshletVertexCount);
            primitive.meshlets.resize(meshletsOffset + clusterCount);
            for (i32 clusterIndex = 0; clusterIndex < clusterCount; clusterIndex++) {
                const clodCluster& cluster = clusters[clusterIndex];
                Meshlet& meshlet = primitive.meshlets[meshletsOffset + clusterIndex];
                meshlet.groupIndex = currentGroupIndex;
                meshlet.boundingSphere.center = glm::make_vec3(&group.simplified.center[0]);
                meshlet.boundingSphere.radius = group.simplified.radius;
                meshlet.clusterError = group.simplified.error;
                meshlet.lod = group.depth;

                meshlet.vertexCount = cluster.vertex_count;
                meshlet.indexCount = cluster.index_count;
                meshlet.indexOffset = meshletIndexOffset;
                meshlet.vertexOffset = meshletVertexOffset;

                std::unique_ptr<unsigned char[]> triangles = std::make_unique<unsigned char[]>(meshlet.indexCount);
                std::size_t uniqueVertices = clodLocalIndices(primitive.meshletVertexIndices.data() + meshletVertexOffset,
                    triangles.get(),
                    cluster.indices,
                    cluster.index_count);
                verify(uniqueVertices == cluster.vertex_count, "uniqueVertices == cluster.vertex_count");
                for (i32 index = 0; index < cluster.index_count; index++) {
                    primitive.meshletIndices[meshletIndexOffset + index] = triangles[index];
                }

                if (cluster.refined != -1) {
                    meshlet.refinedError = groupBounds[cluster.refined].error;
                    meshlet.refinedBoundingSphere.center = glm::make_vec3(&groupBounds[cluster.refined].center[0]);
                    meshlet.refinedBoundingSphere.radius = groupBounds[cluster.refined].radius;
                }

                meshletIndexOffset += cluster.index_count;
                meshletVertexOffset += cluster.vertex_count;
            }
            return currentGroupIndex;
        });
    }

    static void processScene(LoadedScene& scene, const std::string& modelName, const Carrot::NotificationID& loadNotifID) {
        for(std::size_t i = 0; i < scene.primitives.size(); i++) {
            Carrot::UserNotifications::getInstance().setProgress(loadNotifID, float(i) / scene.primitives.size());

            auto& primitive = scene.primitives[i];
            ExpandedMesh expandedMesh = expandMesh(primitive, loadNotifID);

            if(!primitive.hadTexCoords) {
                //TODO; // not supported yet
            }

            if(!primitive.hadNormals) {
                Carrot::Log::info("Mesh %s has no normals, generating flat normals...", primitive.name.c_str());
                generateFlatNormals(expandedMesh, loadNotifID);
                Carrot::Log::info("Mesh %s, generated flat normals!", primitive.name.c_str());
            }

            if(!primitive.hadTangents) {
                Carrot::Log::info("Mesh %s has no tangents, generating tangents...", primitive.name.c_str());
                generateMikkTSpaceTangents(expandedMesh, loadNotifID);
                Carrot::Log::info("Mesh %s, generated tangents!", primitive.name.c_str());
            }

            cleanupTangents(expandedMesh, loadNotifID);

            collapseMesh(primitive, expandedMesh, loadNotifID);
            if(!primitive.vertices.empty()) {
                // TODO: support for skinned meshes
                const float simplifyScale = meshopt_simplifyScale(&primitive.vertices[0].pos.x, primitive.vertices.size(), sizeof(Carrot::Vertex));
                generateClusterHierarchy(primitive, simplifyScale);
            }
        }

        // iterate over nodes, for processes that require the transform of the mesh and/or to handle instances of the same mesh
        // also assigns the nodeKey used to link nodes to data inside the scene
        VulkanHelper vkHelper;
        std::uint32_t nodeKeyValue = 0;
        std::function<void(Carrot::Render::SkeletonTreeNode&, glm::mat4)> iterateOverNodes = [&](Carrot::Render::SkeletonTreeNode& node, const glm::mat4& nodeTransform) {
            node.nodeKey.value = nodeKeyValue++;
            glm::mat4 transform = nodeTransform * node.bone.originalTransform;
            if(node.meshIndices.has_value()) {
                for(const std::size_t meshIndex : node.meshIndices.value()) {
                    prebuildGroupBLASes(vkHelper, scene, node.nodeKey, meshIndex, transform);
                }
            }

            for(auto& child : node.getChildren()) {
                iterateOverNodes(child, transform);
            }
        };
        iterateOverNodes(scene.nodeHierarchy->hierarchy, glm::mat4{ 1.0f });
    }

    static void processGLTFModel(const std::string& modelName, tinygltf::Model& model) {
        Carrot::NotificationID loadNotifID = Carrot::UserNotifications::getInstance().showNotification({.title = Carrot::sprintf("Processing %s", modelName.c_str())});
        CLEANUP(Carrot::UserNotifications::getInstance().closeNotification(loadNotifID));

        GLTFLoader loader{};
        LoadedScene scene = loader.load(model, {});

        processScene(scene, modelName, loadNotifID);

        // re-export model
        tinygltf::Model reexported = std::move(writeAsGLTF(modelName, scene));
        // keep copyright+author info
        model.asset.extras = std::move(model.asset.extras);
        model.asset.copyright = std::move(model.asset.copyright);

        model = std::move(reexported);
    }

    ConversionResult processAssimp(const std::filesystem::path& inputFile, const std::filesystem::path& outputFile) {
        AssimpLoader loader;
        Assimp::Importer importer;
        LoadedScene scene = std::move(loader.load(inputFile.string(), importer));

        const std::string modelName = Carrot::toString(outputFile.stem().u8string());
        {
            Carrot::NotificationID loadNotifID = Carrot::UserNotifications::getInstance().showNotification({.title = Carrot::sprintf("Processing %s", modelName.c_str())});
            CLEANUP(Carrot::UserNotifications::getInstance().closeNotification(loadNotifID));
            processScene(scene, modelName, loadNotifID);
        }

        tinygltf::TinyGLTF gltf;
        tinygltf::Model reexported = std::move(writeAsGLTF(modelName, scene));
        if(!gltf.WriteGltfSceneToFile(&reexported, outputFile.string(), false, false, true/* pretty-print */, false)) {
            return {
                .errorCode = ConversionResultError::ModelCompressionError,
                .errorMessage = "Could not write GLTF",
            };
        }

        return {
            .errorCode = ConversionResultError::Success,
        };
    }


    ConversionResult processGLTF(const std::filesystem::path& inputFile, const std::filesystem::path& outputFile) {
        using namespace tinygltf;

        tinygltf::TinyGLTF parser;
        tinygltf::FsCallbacks callbacks;

        callbacks.ReadWholeFile = gltfReadWholeFile;
        callbacks.ExpandFilePath = gltfExpandFilePath;
        callbacks.FileExists = gltfFileExists;
        callbacks.WriteWholeFile = nullptr;

        fspath parentPath = inputFile.parent_path();
        fspath outputParentPath = outputFile.parent_path();
        callbacks.user_data = (void*)&parentPath;
        parser.SetFsCallbacks(callbacks);

        tinygltf::Model model;
        std::string errors;
        std::string warnings;

        if(inputFile.extension() == ".glb") {
            if(!parser.LoadBinaryFromFile(&model, &errors, &warnings, inputFile.string())) {
                return {
                    .errorCode = ConversionResultError::ModelCompressionError,
                    .errorMessage = errors,
                };
            }
        } else {
            if(!parser.LoadASCIIFromFile(&model, &errors, &warnings, inputFile.string())) {
                return {
                    .errorCode = ConversionResultError::ModelCompressionError,
                    .errorMessage = errors,
                };
            }
        }

        // ----------

        // buffers are regenerated inside 'processModel' method too, so we don't copy the .bin file
        processGLTFModel(Carrot::toString(outputFile.stem().u8string()), model);

        // ----------

        if(!parser.WriteGltfSceneToFile(&model, outputFile.string(), false, false, true/* pretty-print */, false)) {
            return {
                .errorCode = ConversionResultError::ModelCompressionError,
                .errorMessage = "Could not write GLTF",
            };
        }

        return {
            .errorCode = ConversionResultError::Success,
        };
    }
}
