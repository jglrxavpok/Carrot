//
// Created by jglrxavpok on 05/11/2022.
//

#include <gltf/GLTFProcessing.h>
#include <core/utils/CarrotTinyGLTF.h>
#include <core/scene/LoadedScene.h>
#include <core/Macros.h>
#include <core/scene/GLTFLoader.h>
#include <gltf/GLTFWriter.h>
#include <unordered_set>
#include <core/io/Logging.hpp>
#include <glm/gtx/component_wise.hpp>
#include <core/tasks/Tasks.h>
#include <core/data/Hashes.h>
#include <core/containers/KDTree.hpp>
#include <glm/gtx/hash.hpp>

#include <gltf/MikkTSpaceInterface.h>

#include <meshoptimizer.h>

#define IDXTYPEWIDTH 64
#define REALTYPEWIDTH 64
#include <metis.h>
#include <glm/gtx/norm.hpp>

namespace Fertilizer {

    constexpr float Epsilon = 10e-16;
    constexpr const char* const KHR_TEXTURE_BASISU_EXTENSION_NAME = "KHR_texture_basisu";

    using fspath = std::filesystem::path;
    using namespace Carrot::IO;
    using namespace Carrot::Render;

    bool gltfReadWholeFile(std::vector<unsigned char>* out,
                           std::string* err, const std::string& filepath,
                           void* userData) {
        FILE* f;

        CLEANUP(fclose(f));
        if(fopen_s(&f, filepath.c_str(), "rb")) {
            *err = "Could not open file.";
            return false;
        }

        if(fseek(f, 0, SEEK_END)) {
            *err = "Could not seek in file.";
            return false;
        }

        std::size_t size = ftell(f);

        if(fseek(f, 0, SEEK_SET)) {
            *err = "Could not seek in file.";
            return false;
        }

        out->resize(size);
        std::size_t readSize = fread_s(out->data(), size, 1, size, f);
        if(readSize != size) {
            *err = "Could not read file.";
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
    static ExpandedMesh expandMesh(const LoadedPrimitive& primitive) {
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
        glm::i8vec4 boneIDs;
        glm::vec4 boneWeights;

        bool operator==(const VertexKey&) const = default;
    };

    struct VertexKeyHasher {
        std::size_t operator()(const VertexKey& k) const {
            std::size_t h = std::hash<glm::vec3>{}(k.pos);
            Carrot::hash_combine(h, std::hash<glm::vec2>{}(k.uv));
            Carrot::hash_combine(h, std::hash<glm::vec3>{}(k.color));
            Carrot::hash_combine(h, std::hash<glm::i8vec4>{}(k.boneIDs));
            Carrot::hash_combine(h, std::hash<glm::vec4>{}(k.boneWeights));
            return h;
        }
    };

    /**
     * Generate indexed mesh into 'out' from a non-indexed mesh inside ExpandedMesh
     */
    static void collapseMesh(LoadedPrimitive& out, ExpandedMesh& mesh) {
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
                .pos = duplicatedVertex.vertex.pos.xyz,
                .uv = duplicatedVertex.vertex.uv,
                .color = duplicatedVertex.vertex.color,
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

    static void generateFlatNormals(ExpandedMesh& mesh) {
        std::size_t faceCount = mesh.vertices.size() / 3;
        for(std::size_t face = 0; face < faceCount; face++) {
            Carrot::Vertex& a = mesh.vertices[face * 3 + 0].vertex;
            Carrot::Vertex& b = mesh.vertices[face * 3 + 1].vertex;
            Carrot::Vertex& c = mesh.vertices[face * 3 + 2].vertex;

            glm::vec3 ab = (b.pos - a.pos).xyz;
            glm::vec3 bc = (c.pos - b.pos).xyz;
            glm::vec3 ac = (c.pos - a.pos).xyz;

            if(glm::dot(ab, ab) <= Epsilon
            || glm::dot(bc, bc) <= Epsilon
            || glm::dot(ac, ac) <= Epsilon
            ) {
                Carrot::Log::warn("Degenerate triangle (face = %llu)", face);
            }

            a.normal = glm::normalize(glm::cross(ab, ac));
            b.normal = glm::normalize(glm::cross(bc, -ab));
            c.normal = glm::normalize(glm::cross(ac, -bc));
        }
    }

    static void generateMikkTSpaceTangents(ExpandedMesh& mesh) {
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
    static void cleanupTangents(ExpandedMesh& mesh) {
        verify(mesh.vertices.size() % 3 == 0, "Only triangle meshs are supported");
        bool needsRegeneration = false;
        for (std::size_t i = 0; i < mesh.vertices.size(); i += 3) {
            auto isCloseToCollinear = [](const glm::vec3& a, const glm::vec3& b) {
                constexpr float epsilon = 10e-12f;
                const glm::vec3 rejected = b - glm::dot(a, b) * b;
                return glm::all(glm::lessThan(glm::abs(rejected), glm::vec3(epsilon)));
            };

            for(std::size_t j = 0; j < 3; j++) {
                const glm::vec3& normal = mesh.vertices[i + j].vertex.normal;
                const glm::vec3 tangent = mesh.vertices[i + j].vertex.tangent.xyz;
                needsRegeneration |= isCloseToCollinear(normal, tangent);
            }

            if(needsRegeneration) {
                break;
            }
        }

        if(needsRegeneration) {
            Carrot::Log::warn("Found collinear normals and tangents (maybe due to missing UV mapping), generating basic tangents");
            // regenerate all tangents for this mesh, we found collinear normal and tangents
            for (std::size_t i = 0; i < mesh.vertices.size(); i += 3) {
                const glm::vec3 edge = mesh.vertices[i + 1].vertex.pos.xyz - mesh.vertices[i + 0].vertex.pos.xyz;
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
    static std::vector<MeshletGroup> groupMeshlets(LoadedPrimitive& primitive, std::span<Meshlet> meshlets, std::span<const std::int64_t> vertexRemap) {
        // ===== Build meshlet connections
        auto groupWithAllMeshlets = [&]() {
            MeshletGroup group;
            for (int i = 0; i < meshlets.size(); ++i) {
                group.meshlets.push_back(i);
            }
            return std::vector { group };
        };
        if(meshlets.size() < 8) {
            return groupWithAllMeshlets();
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

        // meshlets represented by their index into 'previousLevelMeshlets'
        std::unordered_map<MeshletEdge, std::vector<std::size_t>, MeshletEdgeHasher> edges2Meshlets;
        std::unordered_map<std::size_t, std::vector<MeshletEdge>> meshlets2Edges;

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
                    edges2Meshlets[edge].push_back(meshletIndex);
                    meshlets2Edges[meshletIndex].emplace_back(edge);
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

        std::vector<MeshletGroup> groups;

        idx_t vertexCount = meshlets.size(); // vertex count, from the point of view of METIS, where Meshlet = vertex
        idx_t ncon = 1;
        idx_t nparts = meshlets.size()/4;
        verify(nparts > 1, "Must have at least 2 parts in partition for METIS");
        idx_t options[METIS_NOPTIONS];
        METIS_SetDefaultOptions(options);
        options[METIS_OPTION_OBJTYPE] = METIS_OBJTYPE_CUT;
        options[METIS_OPTION_CCORDER] = 1; // identify connected components first

        std::vector<idx_t> partition;
        partition.resize(vertexCount);

        std::vector<idx_t> xadjacency;
        xadjacency.reserve(vertexCount + 1);

        std::vector<idx_t> edgeAdjacency;
        std::vector<idx_t> edgeWeights;

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
                        auto existingEdgeIter = std::find(edgeAdjacency.begin()+startIndexInEdgeAdjacency, edgeAdjacency.end(), connectedMeshlet);
                        if(existingEdgeIter == edgeAdjacency.end()) {
                            edgeAdjacency.emplace_back(connectedMeshlet);
                            edgeWeights.emplace_back(1);
                        } else {
                            std::ptrdiff_t d = std::distance(edgeAdjacency.begin(), existingEdgeIter);
                            assert(d >= 0);
                            verify(d < edgeWeights.size(), "edgeWeights and edgeAdjacency do not have the same length?");
                            edgeWeights[d]++;
                        }
                    }
                }
            }
            xadjacency.push_back(startIndexInEdgeAdjacency);
        }
        xadjacency.push_back(edgeAdjacency.size());
        verify(xadjacency.size() == meshlets.size() + 1, "unexpected count of vertices for METIS graph?");
        verify(edgeAdjacency.size() == edgeWeights.size(), "edgeWeights and edgeAdjacency must have the same length");

        idx_t edgeCut;
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

        // ===== Group meshlets together
        groups.resize(nparts);
        for(std::size_t i = 0; i < meshlets.size(); i++) {
            idx_t partitionNumber = partition[i];
            groups[partitionNumber].meshlets.push_back(i);
        }
        return groups;
    }

    /// trick to allow getPosition() on Carrot::Vertex for KDTree, without needing to allocate anything
    struct VertexWrapper {
        Carrot::Vertex vertex;

        glm::vec3 getPosition() const {
            return vertex.pos.xyz;
        }
    };

    std::vector<std::int64_t> mergeByDistance(const LoadedPrimitive& primitive, float maxDistance, float maxUVDistance, const Carrot::KDTree<VertexWrapper>& kdtree) {
        // merge vertices which are close enough
        std::vector<std::int64_t> vertexRemap;

        const std::size_t vertexCount = primitive.vertices.size();
        vertexRemap.resize(vertexCount);
        for(auto& v : vertexRemap) {
            v = -1;
        }

        Carrot::Vector<std::size_t> neighbors { Carrot::MallocAllocator::instance };
        for(std::int64_t v = 0; v < vertexCount; v++) {
            neighbors.clear();
            const Carrot::Vertex& currentVertex = primitive.vertices[v];
            const VertexWrapper* pWrappedVertex = reinterpret_cast<const VertexWrapper*>(&currentVertex);
            kdtree.getNeighbors(neighbors, *pWrappedVertex, maxDistance);

            float maxDistanceSq = maxDistance*maxDistance;
            float maxUVDistanceSq = maxUVDistance*maxUVDistance;

            std::int64_t replacement = -1;
            for(const std::size_t& neighbor : neighbors) {
                if(neighbor >= v) {
                    // due to the way we iterate, all indices starting from v will not be remapped yet
                    continue;
                }
                const Carrot::Vertex& otherVertex = primitive.vertices[vertexRemap[neighbor]];
                const float vertexDistanceSq = glm::distance2(currentVertex.pos, otherVertex.pos);
                if(vertexDistanceSq <= maxDistanceSq) {
                    const float uvDistanceSq = glm::distance2(currentVertex.uv, otherVertex.uv);
                    if(uvDistanceSq <= maxUVDistanceSq) {
                        replacement = vertexRemap[neighbor];
                        maxDistanceSq = vertexDistanceSq;
                        maxUVDistanceSq = uvDistanceSq;
                    }
                }
            }

            if(replacement == -1) {
                vertexRemap[v] = v;
            } else {
                vertexRemap[v] = replacement;
            }
        }
        return vertexRemap;
    }

    static void appendMeshlets(LoadedPrimitive& primitive, std::span<std::uint32_t> indexBuffer) {
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
        }, 32);
    }

    /**
     * From this primitive's vertex & index buffer, generate meshlets/clusters
     */
    static void generateClusterHierarchy(LoadedPrimitive& primitive, float simplifyScale) {
        // level 0
        // tell meshoptimizer to generate meshlets
        auto& indexBuffer = primitive.indices;
        std::size_t previousMeshletsStart = 0;
        appendMeshlets(primitive, indexBuffer);

        // TODO: move inside loop (KDtree per LOD level)
        Carrot::KDTree<VertexWrapper> kdtree { Carrot::MallocAllocator::instance };
        static_assert(sizeof(Carrot::Vertex) == sizeof(VertexWrapper));
        std::span<const VertexWrapper> wrappedVertices {
            reinterpret_cast<VertexWrapper*>(primitive.vertices.data()),
            primitive.vertices.size()
        };
        kdtree.build(wrappedVertices);

        // level n+1
        const int maxLOD = 10;
        Carrot::Vector<unsigned int> groupVertexIndices;
        for (int lod = 0; lod < maxLOD; ++lod) {
            float tLod = lod / (float)maxLOD;
            std::span<Meshlet> previousLevelMeshlets = std::span { primitive.meshlets.data() + previousMeshletsStart, primitive.meshlets.size() - previousMeshletsStart };
            if(previousLevelMeshlets.size() <= 1) {
                return; // we have reached the end
            }

            const float maxDistance = (tLod * 0.1f + (1-tLod) * 0.001f) * simplifyScale;
            const float maxUVDistance = tLod * 0.1f + (1-tLod) * 1.0f / 256.0f;
            const std::vector<std::int64_t> vertexRemap = mergeByDistance(primitive, maxDistance, maxUVDistance, kdtree);
            const std::vector<MeshletGroup> groups = groupMeshlets(primitive, previousLevelMeshlets, vertexRemap);

            // ===== Simplify groups
            const std::size_t newMeshletStart = primitive.meshlets.size();
            for(const auto& group : groups) {
                groupVertexIndices.clear();
                // meshlets vector is modified during the loop
                previousLevelMeshlets = std::span { primitive.meshlets.data() + previousMeshletsStart, primitive.meshlets.size() - previousMeshletsStart };

                // add cluster vertices to this group
                for(const auto& meshletIndex : group.meshlets) {
                    const auto& meshlet = previousLevelMeshlets[meshletIndex];

                    std::size_t start = groupVertexIndices.size();
                    groupVertexIndices.resize(start + meshlet.indexCount);
                    for(std::size_t j = 0; j < meshlet.indexCount; j++) {
                        groupVertexIndices[j + start] = vertexRemap[primitive.meshletVertexIndices[primitive.meshletIndices[meshlet.indexOffset + j] + meshlet.vertexOffset]];
                        //groupVertexIndices[j + start] = primitive.meshletVertexIndices[primitive.meshletIndices[meshlet.indexOffset + j] + meshlet.vertexOffset];
                    }
                }

                float targetError = (0.5f * tLod + 0.01f * (1-tLod));

                // simplify this group
                const float threshold = 0.5f;
                std::size_t targetIndexCount = groupVertexIndices.size() * threshold;
                unsigned int options = meshopt_SimplifyLockBorder; // we want all group borders to be locked (because they are shared between groups)

                std::vector<unsigned int> simplifiedIndexBuffer;
                simplifiedIndexBuffer.resize(groupVertexIndices.size());
                float simplificationError = 0.f;

                std::size_t simplifiedIndexCount = meshopt_simplify(simplifiedIndexBuffer.data(), // output
                                                                    groupVertexIndices.data(), groupVertexIndices.size(), // index buffer
                                                                    &primitive.vertices[0].pos.x, primitive.vertices.size(), sizeof(Carrot::Vertex), // vertex buffer
                                                                    targetIndexCount, targetError, options, &simplificationError
                );
                simplifiedIndexBuffer.resize(simplifiedIndexCount);

                // ===== Generate meshlets for this group
                appendMeshlets(primitive, simplifiedIndexBuffer);
            }

            for(std::size_t i = newMeshletStart; i < primitive.meshlets.size(); i++) {
                primitive.meshlets[i].lod = lod + 1;
            }
            previousMeshletsStart = newMeshletStart;
        }
    }

    static void processModel(const std::string& modelName, tinygltf::Model& model) {
        GLTFLoader loader{};
        LoadedScene scene = loader.load(model, {});

        for(auto& primitive : scene.primitives) {
            ExpandedMesh expandedMesh = expandMesh(primitive);

            if(!primitive.hadTexCoords) {
                //TODO; // not supported yet
            }

            if(!primitive.hadNormals) {
                Carrot::Log::info("Mesh %s has no normals, generating flat normals...", primitive.name.c_str());
                generateFlatNormals(expandedMesh);
                Carrot::Log::info("Mesh %s, generated flat normals!", primitive.name.c_str());
            }

            if(!primitive.hadTangents) {
                Carrot::Log::info("Mesh %s has no tangents, generating tangents...", primitive.name.c_str());
                generateMikkTSpaceTangents(expandedMesh);
                Carrot::Log::info("Mesh %s, generated tangents!", primitive.name.c_str());
            }

            cleanupTangents(expandedMesh);

            collapseMesh(primitive, expandedMesh);
            if(!primitive.vertices.empty()) {
                // TODO: support for skinned meshes
                const float simplifyScale = meshopt_simplifyScale(&primitive.vertices[0].pos.x, primitive.vertices.size(), sizeof(Carrot::Vertex));
                generateClusterHierarchy(primitive, simplifyScale);
            }
        }

        // re-export model
        tinygltf::Model reexported = std::move(writeAsGLTF(modelName, scene));
        // keep copyright+author info
        model.asset.extras = std::move(model.asset.extras);
        model.asset.copyright = std::move(model.asset.copyright);

        model = std::move(reexported);
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

        if(!parser.LoadASCIIFromFile(&model, &errors, &warnings, inputFile.string())) {
            return {
                .errorCode = ConversionResultError::GLTFCompressionError,
                .errorMessage = errors,
            };
        }

        // ----------

        // buffers are regenerated inside 'processModel' method too, so we don't copy the .bin file
        processModel(Carrot::toString(outputFile.stem().u8string()), model);

        // ----------

        if(!parser.WriteGltfSceneToFile(&model, outputFile.string(), false, false, true/* pretty-print */, false)) {
            return {
                .errorCode = ConversionResultError::GLTFCompressionError,
                .errorMessage = "Could not write GLTF",
            };
        }

        return {
            .errorCode = ConversionResultError::Success,
        };
    }
}