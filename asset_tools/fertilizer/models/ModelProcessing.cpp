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

#include <meshoptimizer.h>

#define IDXTYPEWIDTH 64
#define REALTYPEWIDTH 64
#include <metis.h>
#include <robin_hood.h>
#include <core/math/Sphere.h>
#include <core/render/VkAccelerationStructureHeader.h>
#include <core/scene/AssimpLoader.h>
#include <glm/gtx/norm.hpp>
#include <gpu_assistance/VulkanHelper.h>

#include "assimp/Importer.hpp"

namespace Fertilizer {

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

    /**
     * Connections betweens meshlets
     */
    struct MeshletEdge {
        explicit MeshletEdge(std::size_t a, std::size_t b): first(std::min(a, b)), second(std::max(a, b)) {}

        bool operator==(const MeshletEdge& other) const = default;

        const std::size_t first;
        const std::size_t second;
    };

    struct MeshletEdgeHasher {
        std::size_t operator()(const MeshletEdge& edge) const {
            std::size_t h = edge.first;
            Carrot::hash_combine(h, edge.second);
            return h;
        }
    };

    /// trick to allow getPosition() on Carrot::Vertex for KDTree, without needing to allocate anything
    struct VertexWrapper {
        const Carrot::Vertex* vertices = nullptr;
        std::size_t index = 0;

        glm::vec3 getPosition() const {
            return vertices[index].pos.xyz();
        }
    };

    /**
     * Find which vertices are part of meshlet boundaries. These should not be merged to avoid cracks between LOD levels
     */
    static Carrot::Vector<bool> findBoundaryVertices(Carrot::Allocator& allocator, LoadedPrimitive& primitive, std::span<Meshlet> meshlets) {
        Carrot::Vector<bool> boundaryVertices { allocator };
        boundaryVertices.resize(primitive.vertices.size());
        boundaryVertices.fill(false);

        // meshlets represented by their index into 'previousLevelMeshlets'
        std::unordered_map<MeshletEdge, std::unordered_set<std::size_t>, MeshletEdgeHasher> edges2Meshlets;

        // for each meshlet
        for(std::size_t meshletIndex = 0; meshletIndex < meshlets.size(); meshletIndex++) {
            const auto& meshlet = meshlets[meshletIndex];
            auto getVertexIndex = [&](std::size_t index) {
                const std::size_t vertexIndex = primitive.meshletVertexIndices[primitive.meshletIndices[index + meshlet.indexOffset] + meshlet.vertexOffset];
                return vertexIndex;
            };

            const std::size_t triangleCount = meshlet.indexCount / 3;
            // for each triangle of the meshlet
            for(std::size_t triangleIndex = 0; triangleIndex < triangleCount; triangleIndex++) {
                // for each edge of the triangle
                for(std::size_t i = 0; i < 3; i++) {
                    MeshletEdge edge { getVertexIndex(i + triangleIndex * 3), getVertexIndex(((i+1) % 3) + triangleIndex * 3) };
                    if(edge.first != edge.second) {
                        edges2Meshlets[edge].insert(meshletIndex);
                    }
                }
            }
        }

        for(const auto& [edge, meshlets] : edges2Meshlets) {
            if(meshlets.size() == 1) {
                boundaryVertices[edge.first] = true;
                boundaryVertices[edge.second] = true;
            }
        }

        return boundaryVertices;
    }

    struct MeshletGroup {
        std::vector<std::size_t> meshlets;
    };
    static Carrot::Vector<MeshletGroup> groupMeshlets(Carrot::Allocator& allocator, LoadedPrimitive& primitive, std::span<Meshlet> meshlets, std::span<const std::int64_t> vertexRemap) {
        // ===== Build meshlet connections
        auto groupWithAllMeshlets = [&]() {
            MeshletGroup group;
            for (int i = 0; i < meshlets.size(); ++i) {
                group.meshlets.push_back(i);
            }
            Carrot::Vector<MeshletGroup> groups { allocator };
            groups.emplaceBack(group);
            return groups;
        };
        if(meshlets.size() < 8) {
            return groupWithAllMeshlets();
        }

        // meshlets represented by their index into 'previousLevelMeshlets'
        std::unordered_map<MeshletEdge, std::unordered_set<std::size_t>, MeshletEdgeHasher> edges2Meshlets;
        std::unordered_map<std::size_t, std::unordered_set<MeshletEdge, MeshletEdgeHasher>> meshlets2Edges;

        // for each meshlet
        for(std::size_t meshletIndex = 0; meshletIndex < meshlets.size(); meshletIndex++) {
            const auto& meshlet = meshlets[meshletIndex];
            auto getVertexIndex = [&](std::size_t index) {
                std::size_t vertexIndex = primitive.meshletVertexIndices[primitive.meshletIndices[index + meshlet.indexOffset] + meshlet.vertexOffset];
                return static_cast<std::size_t>(vertexRemap[vertexIndex]);
            };

            const std::size_t triangleCount = meshlet.indexCount / 3;
            // for each triangle of the meshlet
            for(std::size_t triangleIndex = 0; triangleIndex < triangleCount; triangleIndex++) {
                // for each edge of the triangle
                for(std::size_t i = 0; i < 3; i++) {
                    MeshletEdge edge { getVertexIndex(i + triangleIndex * 3), getVertexIndex(((i+1) % 3) + triangleIndex * 3) };
                    if(edge.first != edge.second) {
                        edges2Meshlets[edge].insert(meshletIndex);
                        meshlets2Edges[meshletIndex].insert(edge);
                    }
                }
            }
        }

        // remove edges which are not connected to 2 different meshlets
        std::erase_if(edges2Meshlets, [&](const auto& pair) {
            return pair.second.size() <= 1;
        });

        if(edges2Meshlets.empty()) {
            return groupWithAllMeshlets();
        }

        // at this point, we have basically built a graph of meshlets, in which edges represent which meshlets are connected together

        Carrot::Vector<MeshletGroup> groups { allocator };

        idx_t vertexCount = meshlets.size(); // vertex count, from the point of view of METIS, where Meshlet = vertex
        idx_t ncon = 1;
        idx_t nparts = meshlets.size()/4;
        verify(nparts > 1, "Must have at least 2 parts in partition for METIS");
        idx_t options[METIS_NOPTIONS];
        METIS_SetDefaultOptions(options);
        options[METIS_OPTION_OBJTYPE] = METIS_OBJTYPE_CUT;
        options[METIS_OPTION_CCORDER] = 1; // identify connected components first
        options[METIS_OPTION_NUMBERING] = 0;

        Carrot::Vector<idx_t> partition { allocator };
        partition.resize(vertexCount);

        Carrot::Vector<idx_t> xadjacency { allocator };
        xadjacency.setCapacity(vertexCount + 1);

        Carrot::Vector<idx_t> edgeAdjacency { allocator };
        Carrot::Vector<idx_t> edgeWeights { allocator };
        edgeAdjacency.setGrowthFactor(2);
        edgeWeights.setGrowthFactor(2);

        for(std::size_t meshletIndex = 0; meshletIndex < meshlets.size(); meshletIndex++) {
            std::size_t startIndexInEdgeAdjacency = edgeAdjacency.size();
            for(const auto& edge : meshlets2Edges[meshletIndex]) {
                auto connectionsIter = edges2Meshlets.find(edge);
                if(connectionsIter == edges2Meshlets.end()) {
                    continue;
                }
                const auto& connections = connectionsIter->second;
                for(const auto& connectedMeshlet : connections) {
                    if(connectedMeshlet != meshletIndex) {
                        auto existingEdgeIter = edgeAdjacency.find(connectedMeshlet, startIndexInEdgeAdjacency);
                        if(existingEdgeIter == edgeAdjacency.end()) {
                            verify(edgeAdjacency.size() == edgeWeights.size(), "edgeWeights and edgeAdjacency must have the same length");
                            edgeAdjacency.emplaceBack(connectedMeshlet);
                            edgeWeights.emplaceBack(1);
                        } else {
                            std::ptrdiff_t d = existingEdgeIter - edgeAdjacency.begin();
                            assert(d >= 0);
                            verify(d < edgeWeights.size(), "edgeWeights and edgeAdjacency do not have the same length?");
                            edgeWeights[d]++;
                        }
                    }
                }
            }
            xadjacency.pushBack(startIndexInEdgeAdjacency);
        }
        xadjacency.pushBack(edgeAdjacency.size());
        verify(xadjacency.size() == vertexCount + 1, "unexpected count of vertices for METIS graph?");

        // coherency checks
#if 0
        for(const std::size_t& edgeAdjIndex : xadjacency) {
            verify(edgeAdjIndex <= edgeAdjacency.size(), "Out of bounds inside xadjacency");
        }
        for(const std::size_t& vertexIndex : edgeAdjacency) {
            verify(vertexIndex <= vertexCount, "Out of bounds inside edgeAdjacency");
        }
#endif

        static Carrot::Async::SpinLock metisLock {};
        idx_t edgeCut;
        {
            Carrot::Async::LockGuard g { metisLock };
            int result = METIS_PartGraphKway(&vertexCount,
                                             &ncon,
                                             xadjacency.data(),
                                             edgeAdjacency.data(),
                                             nullptr, /* vertex weights */
                                             nullptr, /* vertex size */
                                             edgeWeights.data(),
                                             &nparts,
                                             nullptr,
                                             nullptr,
                                             options,
                                             &edgeCut,
                                             partition.data()
                                );
            verify(result == METIS_OK, "Graph partitioning failed!");
        }

        // ===== Group meshlets together
        groups.resize(nparts);
        for(std::size_t i = 0; i < meshlets.size(); i++) {
            idx_t partitionNumber = partition[i];
            groups[partitionNumber].meshlets.push_back(i);
        }
        return groups;
    }

    std::vector<std::int64_t> mergeByDistance(const LoadedPrimitive& primitive, const Carrot::Vector<bool>& boundary, std::span<const VertexWrapper> groupVerticesPreWeld, float maxDistance, float maxUVDistance, const Carrot::KDTree<VertexWrapper>& kdtree) {
        // merge vertices which are close enough
        std::vector<std::int64_t> vertexRemap;

        const std::size_t vertexCount = primitive.vertices.size();
        vertexRemap.resize(vertexCount);
        for(auto& v : vertexRemap) {
            v = -1;
        }

        Carrot::Vector<Carrot::Vector<std::size_t>> neighborsForAllVertices { Carrot::MallocAllocator::instance };
        neighborsForAllVertices.setCapacity(groupVerticesPreWeld.size());

#if 0
        struct SimilarityKey {
            std::int32_t x;
            std::int32_t y;
            std::int32_t z;
            std::int32_t u;
            std::int32_t v;

            bool operator==(const SimilarityKey& o) const {
                return x == o.x
                && y == o.y
                && z == o.z
                && u == o.u
                && v == o.v;
            }
        };

        struct SimilarityHasher {
            std::size_t operator()(const SimilarityKey& k) const {
                return robin_hood::hash_bytes(&k, sizeof(k));
            }
        };
        std::unordered_map<SimilarityKey, Carrot::Vector<std::size_t>, SimilarityHasher> similarVertices;

        for(std::size_t vertexIndex = 0; vertexIndex < groupVerticesPreWeld.size(); vertexIndex++) {
            auto quantize = [](float value, float quantum) -> std::int32_t {
                return static_cast<std::int32_t>(std::ceil(value / quantum));
            };
            SimilarityKey key{};
            const std::size_t globalIndex = groupVerticesPreWeld[vertexIndex].index;
            const Carrot::Vertex& vertex = primitive.vertices[globalIndex];
            key.x = quantize(vertex.pos.x, maxDistance);
            key.y = quantize(vertex.pos.y, maxDistance);
            key.z = quantize(vertex.pos.z, maxDistance);
            key.u = quantize(vertex.uv.x, maxUVDistance);
            key.v = quantize(vertex.uv.y, maxUVDistance);

            auto [iter, wasNew] = similarVertices.try_emplace(key);
            if(wasNew) {
                iter->second.setGrowthFactor(1.5f);
            }

            iter->second.pushBack(globalIndex);
        }

        for(const auto& [_, cellVertices] : similarVertices) {
            verify(!cellVertices.empty(), "No vertices in cell?");
            for(const std::size_t& vertexIndex : cellVertices) {
                const bool isBoundary = boundary[vertexIndex];
                vertexRemap[vertexIndex] = isBoundary ? vertexIndex : cellVertices[0];
            }
        }
#elif 1
        for(std::size_t v = 0; v < groupVerticesPreWeld.size(); v++) {
            neighborsForAllVertices.emplaceBack(Carrot::MallocAllocator::instance);
            neighborsForAllVertices[v].setGrowthFactor(1.5f);
        }
        Carrot::Async::parallelFor(groupVerticesPreWeld.size(), [&](std::size_t v) {
            const VertexWrapper& currentVertexWrapped = groupVerticesPreWeld[v];
            if(boundary[currentVertexWrapped.index]) {
                return; // no need to compute neighbors
            }
            kdtree.getNeighbors(neighborsForAllVertices[v], currentVertexWrapped, maxDistance);
        }, (groupVerticesPreWeld.size()+31) / 32);
        for(std::int64_t v = 0; v < groupVerticesPreWeld.size(); v++) {
            std::int64_t replacement = -1;
            const auto& currentVertexWrapped = groupVerticesPreWeld[v];
            if(!boundary[currentVertexWrapped.index]) { // boundary vertices must not be merged with others (to avoid cracks)
                auto& neighbors = neighborsForAllVertices[v];
                const Carrot::Vertex& currentVertex = primitive.vertices[currentVertexWrapped.index];

                float maxDistanceSq = maxDistance*maxDistance;
                float maxUVDistanceSq = maxUVDistance*maxUVDistance;

                for(const std::size_t& neighbor : neighbors) {
                    if(vertexRemap[groupVerticesPreWeld[neighbor].index] == -1) {
                        // due to the way we iterate, all indices starting from v will not be remapped yet
                        continue;
                    }
                    const Carrot::Vertex& otherVertex = primitive.vertices[vertexRemap[groupVerticesPreWeld[neighbor].index]];
                    const float vertexDistanceSq = glm::distance2(currentVertex.pos, otherVertex.pos);
                    if(vertexDistanceSq <= maxDistanceSq) {
                        const float uvDistanceSq = glm::distance2(currentVertex.uv, otherVertex.uv);
                        if(uvDistanceSq <= maxUVDistanceSq) {
                            replacement = vertexRemap[groupVerticesPreWeld[neighbor].index];
                            maxDistanceSq = vertexDistanceSq;
                            maxUVDistanceSq = uvDistanceSq;
                        }
                    }
                }
            }

            if(replacement == -1) {
                vertexRemap[currentVertexWrapped.index] = currentVertexWrapped.index;
            } else {
                vertexRemap[currentVertexWrapped.index] = replacement;
            }
        }
#else
        for(std::int64_t v = 0; v < groupVerticesPreWeld.size(); v++) {
            std::int64_t replacement = -1;
            const auto& currentVertexWrapped = groupVerticesPreWeld[v];
            vertexRemap[currentVertexWrapped.index] = currentVertexWrapped.index;
        }
#endif
        return vertexRemap;
    }

    static void appendMeshlets(LoadedPrimitive& primitive, std::span<std::uint32_t> indexBuffer, const Carrot::Math::Sphere& clusterBounds, float clusterError, std::uint32_t* pUniqueGroupIndex) {
        constexpr std::size_t maxVertices = 64;
        constexpr std::size_t maxTriangles = 128;
        const float coneWeight = 0.0f; // for occlusion culling, currently unused

        const std::size_t meshletOffset = primitive.meshlets.size();
        const std::size_t vertexOffset = primitive.meshletVertexIndices.size();
        const std::size_t indexOffset = primitive.meshletIndices.size();
        const std::size_t maxMeshlets = meshopt_buildMeshletsBound(indexBuffer.size(), maxVertices, maxTriangles);
        std::vector<meshopt_Meshlet> meshoptMeshlets;
        meshoptMeshlets.resize(maxMeshlets);

        std::vector<unsigned int> meshletVertexIndices;
        std::vector<unsigned char> meshletTriangles;
        meshletVertexIndices.resize(maxMeshlets * maxVertices);
        meshletTriangles.resize(maxMeshlets * maxVertices * 3);

        const std::size_t meshletCount = meshopt_buildMeshlets(meshoptMeshlets.data(), meshletVertexIndices.data(), meshletTriangles.data(), // meshlet outputs
                                                               indexBuffer.data(), indexBuffer.size(), // original index buffer
                                                               &primitive.vertices[0].pos.x, // pointer to position data
                                                               primitive.vertices.size(), // vertex count of original mesh
                                                               sizeof(Carrot::Vertex), // stride
                                                               maxVertices, maxTriangles, coneWeight);
        const meshopt_Meshlet& last = meshoptMeshlets[meshletCount - 1];
        const std::size_t vertexCount = last.vertex_offset + last.vertex_count;
        const std::size_t indexCount = last.triangle_offset + ((last.triangle_count * 3 + 3) & ~3);
        primitive.meshletVertexIndices.resize(vertexOffset + vertexCount);
        primitive.meshletIndices.resize(indexOffset + indexCount);
        primitive.meshlets.resize(meshletOffset + meshletCount); // remove over-allocated meshlets

        Carrot::Async::parallelFor(vertexCount, [&](std::size_t index) {
            primitive.meshletVertexIndices[vertexOffset + index] = meshletVertexIndices[index];
        }, 1024);
        Carrot::Async::parallelFor(indexCount, [&](std::size_t index) {
            primitive.meshletIndices[indexOffset + index] = meshletTriangles[index];
        }, 1024);


        // meshlets are ready, process them in the format used by Carrot:
        Carrot::Async::parallelFor(meshletCount, [&](std::size_t index) {
            auto& meshoptMeshlet = meshoptMeshlets[index];
            auto& carrotMeshlet = primitive.meshlets[meshletOffset + index];

            carrotMeshlet.vertexOffset = vertexOffset + meshoptMeshlet.vertex_offset;
            carrotMeshlet.vertexCount = meshoptMeshlet.vertex_count;

            carrotMeshlet.indexOffset = indexOffset + meshoptMeshlet.triangle_offset;
            carrotMeshlet.indexCount = meshoptMeshlet.triangle_count*3;
            carrotMeshlet.groupIndex = (*pUniqueGroupIndex)++;

            carrotMeshlet.boundingSphere = clusterBounds;
            carrotMeshlet.clusterError = clusterError;
        }, 32);
    }

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
        // level 0
        // tell meshoptimizer to generate meshlets
        auto& indexBuffer = primitive.indices;
        std::size_t previousMeshletsStart = 0;
        std::uint32_t uniqueGroupIndex = 0;
        {
            glm::vec3 min { +INFINITY, +INFINITY, +INFINITY };
            glm::vec3 max { -INFINITY, -INFINITY, -INFINITY };

            // remap simplified index buffer to mesh-wide vertex indices
            for(auto& index : indexBuffer) {
                const glm::vec3 vertexPos = glm::vec3 { primitive.vertices[index].pos.xyz() };
                min = glm::min(min, vertexPos);
                max = glm::max(max, vertexPos);
            }

            Carrot::Math::Sphere lod0Bounds;
            lod0Bounds.loadFromAABB(min, max);
            appendMeshlets(primitive, indexBuffer, lod0Bounds, 0.0f, &uniqueGroupIndex);
        }

        Carrot::KDTree<VertexWrapper> kdtree { Carrot::MallocAllocator::instance };

        // level n+1
        const int maxLOD = 25;
        Carrot::Vector<unsigned int> groupVertexIndices;
        Carrot::Vector<VertexWrapper> groupVerticesPreWeld;

        // TODO: move higher in call chain
        // allocator used for meshlet grouping, used to reuse memory between passes
        Carrot::StackAllocator groupingAllocator { Carrot::MallocAllocator::instance };
        for (int lod = 0; lod < maxLOD; ++lod) {
            Carrot::UserNotifications::getInstance().setBody(notificationID, Carrot::sprintf("Generating LOD %d", lod+1));
            float tLod = lod / (float)maxLOD;
            std::span<Meshlet> previousLevelMeshlets = std::span { primitive.meshlets.data() + previousMeshletsStart, primitive.meshlets.size() - previousMeshletsStart };
            if(previousLevelMeshlets.size() <= 1) {
                return; // we have reached the end
            }

            std::unordered_set<std::size_t> meshletVertexIndices;

            for(const auto& meshlet : previousLevelMeshlets) {
                auto getVertexIndex = [&](std::size_t index) {
                    return primitive.meshletVertexIndices[primitive.meshletIndices[index + meshlet.indexOffset] + meshlet.vertexOffset];
                };

                const std::size_t triangleCount = meshlet.indexCount / 3;
                // for each triangle of the meshlet
                for(std::size_t i = 0; i < meshlet.indexCount; i++) {
                    meshletVertexIndices.insert(getVertexIndex(i));
                }
            }

            groupVerticesPreWeld.clear();
            groupVerticesPreWeld.ensureReserve(meshletVertexIndices.size());
            for(const std::size_t i : meshletVertexIndices) {
                groupVerticesPreWeld.emplaceBack(primitive.vertices.data(), i);
            }

            std::span<const VertexWrapper> wrappedVertices = groupVerticesPreWeld;
            kdtree.build(wrappedVertices);

            const float maxDistance = (tLod * 0.1f + (1-tLod) * 0.01f) * simplifyScale;
            const float maxUVDistance = tLod * 0.5f + (1-tLod) * 1.0f / 256.0f;
            Carrot::UserNotifications::getInstance().setBody(notificationID, Carrot::sprintf("Generating LOD %d - Merge vertices", lod+1));

            Carrot::StackAllocator tempAllocator { Carrot::Allocator::getDefault() };
            Carrot::Vector<bool> boundary = findBoundaryVertices(tempAllocator, primitive, previousLevelMeshlets);

            const std::vector<std::int64_t> mergeVertexRemap = mergeByDistance(primitive, boundary, groupVerticesPreWeld, maxDistance, maxUVDistance, kdtree);

            groupingAllocator.clear();
            Carrot::UserNotifications::getInstance().setBody(notificationID, Carrot::sprintf("Generating LOD %d - Group clusters", lod+1));
            const Carrot::Vector<MeshletGroup> groups = groupMeshlets(groupingAllocator, primitive, previousLevelMeshlets, mergeVertexRemap);

            // ===== Simplify groups
            const std::size_t newMeshletStart = primitive.meshlets.size();
            Carrot::UserNotifications::getInstance().setBody(notificationID, Carrot::sprintf("Generating LOD %d - Simplify cluster groups", lod+1));
            for(std::size_t groupIndex = 0; groupIndex < groups.size(); groupIndex++) {
                const std::uint32_t currentGroupIndex = uniqueGroupIndex++;
                const auto& group = groups[groupIndex];
                Carrot::UserNotifications::getInstance().setProgress(notificationID, groupIndex/static_cast<float>(groups.size()));
                groupVertexIndices.clear();
                // meshlets vector is modified during the loop
                previousLevelMeshlets = std::span { primitive.meshlets.data() + previousMeshletsStart, primitive.meshlets.size() - previousMeshletsStart };

                Carrot::Vector<Carrot::Vertex> groupVertexBuffer {};
                groupVertexBuffer.setGrowthFactor(1.5f);
                Carrot::Vector<std::size_t> group2meshVertexRemap {};
                std::unordered_map<std::size_t, std::size_t> mesh2groupVertexRemap {};

                // add cluster vertices to this group
                // and remove clusters from clusters to merge
                for(const auto& meshletIndex : group.meshlets) {
                    auto& meshlet = previousLevelMeshlets[meshletIndex];
                    meshlet.groupIndex = currentGroupIndex;

                    std::size_t start = groupVertexIndices.size();
                    groupVertexIndices.ensureReserve(start + meshlet.indexCount);
                    for(std::size_t j = 0; j < meshlet.indexCount; j += 3) { // triangle per triangle
                        std::int64_t triangle[3] = {
                            mergeVertexRemap[primitive.meshletVertexIndices[primitive.meshletIndices[meshlet.indexOffset + j + 0] + meshlet.vertexOffset]],
                            mergeVertexRemap[primitive.meshletVertexIndices[primitive.meshletIndices[meshlet.indexOffset + j + 1] + meshlet.vertexOffset]],
                            mergeVertexRemap[primitive.meshletVertexIndices[primitive.meshletIndices[meshlet.indexOffset + j + 2] + meshlet.vertexOffset]],
                        };

                        // remove triangles which have collapsed on themselves due to vertex merge
                        if(triangle[0] == triangle[1] && triangle[0] == triangle[2]) {
                            continue;
                        }

                        for(std::size_t vertex = 0; vertex < 3; vertex++) {
                            const std::size_t vertexIndex = triangle[vertex];

                            // map vertex index valid for entire to a smaller vertex buffer just for this group
                            auto [iter, bWasNew] = mesh2groupVertexRemap.try_emplace(vertexIndex);
                            if(bWasNew) {
                                iter->second = groupVertexBuffer.size();
                                groupVertexBuffer.emplaceBack(primitive.vertices[iter->second]);
                            }
                            groupVertexIndices.pushBack(iter->second);
                        }
                    }
                }

                if(groupVertexIndices.empty()) {
                    continue;
                }

                // create reverse mapping from group to mesh-wide vertex indices
                group2meshVertexRemap.resize(groupVertexBuffer.size());
                group2meshVertexRemap.fill(~0ull);

                for(const auto& [meshIndex, groupIndex] : mesh2groupVertexRemap) {
                    verify(groupIndex < group2meshVertexRemap.size(), "Wrong size!");
                    group2meshVertexRemap[groupIndex] = meshIndex;
                }

                float targetError = (0.1f * tLod + 0.01f * (1-tLod));

                // simplify this group
                const float threshold = 0.5f;
                std::size_t targetIndexCount = groupVertexIndices.size() * threshold;
                unsigned int options = meshopt_SimplifyLockBorder; // we want all group borders to be locked (because they are shared between groups)

                std::vector<unsigned int> simplifiedIndexBuffer;
                simplifiedIndexBuffer.resize(groupVertexIndices.size());
                float simplificationError = 0.f;

                std::size_t simplifiedIndexCount = meshopt_simplify(simplifiedIndexBuffer.data(), // output
                                                                    groupVertexIndices.data(), groupVertexIndices.size(), // index buffer
                                                                    &groupVertexBuffer[0].pos.x, groupVertexBuffer.size(), sizeof(Carrot::Vertex), // vertex buffer
                                                                    targetIndexCount, targetError, options, &simplificationError
                );
                simplifiedIndexBuffer.resize(simplifiedIndexCount);

                // ===== Generate meshlets for this group
                // TODO: if cluster is not simplified, use it for next LOD
                if(simplifiedIndexCount > 0 && simplifiedIndexCount != groupVertexIndices.size()) {
                    float localScale = meshopt_simplifyScale(&groupVertexBuffer[0].pos.x, groupVertexBuffer.size(), sizeof(Carrot::Vertex));
                    // TODO: numerical stability
                    float meshSpaceError = simplificationError * localScale;
                    float parentError = 0.0f;

                    glm::vec3 min { +INFINITY, +INFINITY, +INFINITY };
                    glm::vec3 max { -INFINITY, -INFINITY, -INFINITY };

                    // remap simplified index buffer to mesh-wide vertex indices
                    for(auto& index : simplifiedIndexBuffer) {
                        index = group2meshVertexRemap[index];

                        const glm::vec3 vertexPos = glm::vec3 { primitive.vertices[index].pos.xyz() };
                        min = glm::min(min, vertexPos);
                        max = glm::max(max, vertexPos);
                    }

                    Carrot::Math::Sphere simplifiedClusterBounds;
                    simplifiedClusterBounds.loadFromAABB(min, max);

                    for(const auto& meshletIndex : group.meshlets) {
                        const auto& previousMeshlet = previousLevelMeshlets[meshletIndex];
                        // ensure parent(this) error >= child(members of group) error
                        parentError = std::max(parentError, previousMeshlet.clusterError);
                    }

                    meshSpaceError += parentError;
                    for(const auto& meshletIndex : group.meshlets) {
                        previousLevelMeshlets[meshletIndex].parentError = meshSpaceError;
                        previousLevelMeshlets[meshletIndex].parentBoundingSphere = simplifiedClusterBounds;
                    }

                    // group index is replaced on next iteration of loop (if there is one) to group the meshlets together based on partitionning
                    appendMeshlets(primitive, simplifiedIndexBuffer, simplifiedClusterBounds, meshSpaceError, &uniqueGroupIndex);
                }
            }

            for(std::size_t i = newMeshletStart; i < primitive.meshlets.size(); i++) {
                primitive.meshlets[i].lod = lod + 1;
            }
            previousMeshletsStart = newMeshletStart;
        }

        // reindex groups inside glTF to ensure contiguous indices:
        const std::uint32_t uninitValue = std::numeric_limits<std::uint32_t>::max();
        Carrot::Vector<std::uint32_t> groupRemap;
        groupRemap.resize(uniqueGroupIndex);
        groupRemap.fill(uninitValue);
        std::uint32_t nextIndex = 0;
        for(auto& meshlet : primitive.meshlets) {
            std::uint32_t& remapValue = groupRemap[meshlet.groupIndex];
            if(remapValue == uninitValue) {
                remapValue = nextIndex++;
            }
            meshlet.groupIndex = remapValue;
        }
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
