//
// Created by jglrxavpok on 17/12/2022.
//

#include "GLTFWriter.h"
#include "core/utils/stringmanip.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/detail/type_quat.hpp"
#include "glm/gtx/matrix_decompose.hpp"
#include "core/scene/GLTFLoader.h" // for extension names

namespace Fertilizer {
    static glm::mat4 carrotSpaceToGLTFSpace = glm::rotate(glm::mat4{1.0f}, -glm::pi<float>()/2.0f, glm::vec3(1,0,0));

    struct Payload {
        tinygltf::Model& glTFModel;

        bool exportedNodes = false;
        std::unordered_map<Carrot::IO::VFS::Path, int> textureIndices;

        Carrot::Render::Skeleton hierarchy{glm::mat4(1.0f)};
        std::unordered_map<const Carrot::Render::SkeletonTreeNode*, std::size_t> nodeMap; // pointers to nodes inside 'hierarchy'
        std::unordered_map<int, int> boneRemap; //< node ID to joint ID for joints
        std::unordered_map<int, int> reverseBoneRemap; //< joint ID to node ID for joints
    };

    static std::vector<double> vectorOfDoubles(const glm::vec2& v) {
        return { v.x, v.y };
    }

    static std::vector<double> vectorOfDoubles(const glm::vec3& v) {
        return { v.x, v.y, v.z };
    }

    static std::vector<double> vectorOfDoubles(const glm::vec4& v) {
        return { v.x, v.y, v.z, v.w };
    }

    static std::vector<double> vectorOfDoubles(const glm::quat& q) {
        return { q.x, q.y, q.z, q.w };
    }

    /**
     * Writes samplers, images and textures to the glTF model, and fills 'payload' with indexing info for textures
     */
    static void writeTextures(Payload& payload, const Carrot::Render::LoadedScene& scene) {
        tinygltf::Model& model = payload.glTFModel;
        model.samplers.emplace_back(); // add a default sampler (unused by Carrot)

        // Carrot has 1:1 mapping between images and textures
        std::size_t textureCount = 0;
        auto addTexture = [&](const Carrot::IO::VFS::Path& path) {
            if(path.isEmpty()) {
                return;
            }
            verify(path.isGeneric(), "Fertilizer does not have a full VFS, all paths should be generic");

            auto [it, wasNew] = payload.textureIndices.insert({ path, textureCount });
            if(wasNew) {
                textureCount++;
            }
        };
        for(const auto& material : scene.materials) {
            addTexture(material.albedo);
            addTexture(material.occlusion);
            addTexture(material.normalMap);
            addTexture(material.metallicRoughness);
            addTexture(material.emissive);
        }

        model.images.resize(textureCount);
        model.textures.resize(textureCount);
        for(const auto& [path, index] : payload.textureIndices) {
            model.images[index].uri = path.toString();

            model.textures[index].name = path.toString(); // to help for debuggability
            model.textures[index].source = index;
            model.textures[index].sampler = 0;
        }
    }

    static tinygltf::TextureInfo makeTextureInfo(const Payload& payload, const Carrot::IO::VFS::Path& texturePath) {
        if(texturePath.isEmpty()) {
            tinygltf::TextureInfo r;
            r.index = -1;
            return r;
        }
        auto it = payload.textureIndices.find(texturePath);
        verify(it != payload.textureIndices.end(), Carrot::sprintf("Programming error: a texture was referenced by scene, but not written to Payload, (%s)", texturePath.toString().c_str()));

        int index = it->second;
        tinygltf::TextureInfo r;
        r.index = index;
        return r;
    }

    static void writeMaterials(Payload& payload, const Carrot::Render::LoadedScene& scene) {
        tinygltf::Model& model = payload.glTFModel;
        std::size_t materialCount = scene.materials.size();
        model.materials.resize(materialCount);

        for (std::size_t i = 0; i < materialCount; ++i) {
            tinygltf::Material& glTFMaterial = model.materials[i];
            const Carrot::Render::LoadedMaterial& material = scene.materials[i];

            switch(material.blendMode) {
                case Carrot::Render::LoadedMaterial::BlendMode::Blend:
                    glTFMaterial.alphaMode = "BLEND";
                    break;

                case Carrot::Render::LoadedMaterial::BlendMode::None:
                    glTFMaterial.alphaMode = "OPAQUE";
                    break;

                default:
                    verify(false, Carrot::sprintf("Unsupported blend type: %d", material.blendMode)); // unsupported type
            }

            glTFMaterial.name = material.name;

            glTFMaterial.pbrMetallicRoughness.baseColorFactor = vectorOfDoubles(material.baseColorFactor);
            glTFMaterial.pbrMetallicRoughness.roughnessFactor = material.roughnessFactor;
            glTFMaterial.pbrMetallicRoughness.metallicFactor = material.metallicFactor;

            glTFMaterial.emissiveFactor = vectorOfDoubles(material.emissiveFactor);

            // unsupported by Carrot
            // TODO: show warning in GLTFLoader ?
            {
                glTFMaterial.normalTexture.scale;
                glTFMaterial.normalTexture.texCoord;
                glTFMaterial.occlusionTexture.texCoord;
            }

            glTFMaterial.pbrMetallicRoughness.baseColorTexture = makeTextureInfo(payload, material.albedo);
            glTFMaterial.pbrMetallicRoughness.metallicRoughnessTexture = makeTextureInfo(payload, material.metallicRoughness);
            glTFMaterial.emissiveTexture = makeTextureInfo(payload, material.emissive);
            glTFMaterial.normalTexture.index = makeTextureInfo(payload, material.normalMap).index;
            glTFMaterial.occlusionTexture.index = makeTextureInfo(payload, material.occlusion).index;
        }
    }

    static void writeMeshes(const std::string& modelName, Payload& payload, const Carrot::Render::LoadedScene& scene) {
        tinygltf::Model& model = payload.glTFModel;
        model.meshes.reserve(scene.primitives.size());

        int staticVertexBufferIndex = -1;
        int staticVertexBufferViewIndex = -1;
        int skinnedVertexBufferIndex = -1;
        int skinnedVertexBufferViewIndex = -1;

        // meshlets buffer
        int meshletBufferIndex = (int)model.buffers.size();
        int meshletBufferViewIndex = (int)model.bufferViews.size();
        {
            auto& meshletBuffer = model.buffers.emplace_back();
            meshletBuffer.uri = modelName + "-meshlets.bin";
            model.bufferViews.emplace_back();
        }
        int clustersBufferIndex = (int)model.buffers.size();
        int clustersBufferViewIndex = (int)model.bufferViews.size();
        {
            auto& clustersBuffer = model.buffers.emplace_back();
            clustersBuffer.uri = modelName + "-cluster-data.bin";
            model.bufferViews.emplace_back();
        }

        // index buffer
        int indexBufferIndex = (int)model.buffers.size();
        int indexBufferViewIndex = (int)model.bufferViews.size();
        auto& indexBuffer = model.buffers.emplace_back();
        indexBuffer.uri = modelName + "-indices.bin";
        model.bufferViews.emplace_back();

        for(const auto& primitive : scene.primitives) {
            if(primitive.isSkinned) {
                if(skinnedVertexBufferIndex == -1) {
                    skinnedVertexBufferIndex = model.buffers.size();
                    skinnedVertexBufferViewIndex = model.bufferViews.size();
                    auto& buffer = model.buffers.emplace_back();
                    buffer.uri = modelName + "-skinned-vertices.bin";
                    model.bufferViews.emplace_back();
                }
            } else {
                if(staticVertexBufferIndex == -1) {
                    staticVertexBufferIndex = model.buffers.size();
                    staticVertexBufferViewIndex = model.bufferViews.size();
                    auto& buffer = model.buffers.emplace_back();
                    buffer.uri = modelName + "-static-vertices.bin";
                    model.bufferViews.emplace_back();
                }
            }
        }

        tinygltf::Buffer& indicesBuffer = model.buffers[indexBufferIndex];
        tinygltf::Buffer* pStaticGeometryBuffer = staticVertexBufferIndex == -1 ? nullptr : &model.buffers[staticVertexBufferIndex];
        tinygltf::Buffer* pSkinnedGeometryBuffer = skinnedVertexBufferIndex == -1 ? nullptr : &model.buffers[skinnedVertexBufferIndex];
        if(pStaticGeometryBuffer != nullptr) {
            pStaticGeometryBuffer->name = "StaticVertexBuffer";
        }
        if(pSkinnedGeometryBuffer != nullptr) {
            pSkinnedGeometryBuffer->name = "SkinnedVertexBuffer";
        }
        indicesBuffer.name = "IndexBuffer";


        std::unordered_map<int, const Carrot::Render::SkeletonTreeNode*> reverseBoneMapping;
        for(const auto& [_, boneMapping] : scene.boneMapping) {
            for(const auto& [k, v] : boneMapping) {
                reverseBoneMapping[v] = payload.hierarchy.findNode(k);
            }
        }

        std::uint32_t accessorIndex = model.accessors.size();
        for(const auto& primitive : scene.primitives) {
            const bool isSkinned = primitive.isSkinned;
            const std::size_t vertexSize = isSkinned ? sizeof(Carrot::SkinnedVertex) : sizeof(Carrot::Vertex);
            const std::size_t vertexCount = isSkinned ? primitive.skinnedVertices.size() : primitive.vertices.size();
            const void* vertexData = nullptr;
            std::vector<Carrot::SkinnedVertex> skinnedVerticesWithRemappedBoneIDs;

            if(isSkinned) {
                // TODO: //-ize
                skinnedVerticesWithRemappedBoneIDs.resize(primitive.skinnedVertices.size());
                for(std::size_t i = 0; i < primitive.skinnedVertices.size(); i++) {
                    auto& vertex = skinnedVerticesWithRemappedBoneIDs[i];
                    const auto& originalVertex = primitive.skinnedVertices[i];
                    vertex = originalVertex;

                    auto remap = [&](int originalBoneID) {
                        auto reverseBoneIter = reverseBoneMapping.find(originalBoneID);
                        if(reverseBoneIter == reverseBoneMapping.end()) {
                            return 0;
                        }
                        const Carrot::Render::SkeletonTreeNode* pTreeNode = reverseBoneIter->second;
                        const int boneID = payload.nodeMap.at(pTreeNode);
                        auto remapIter = payload.boneRemap.find(boneID);
                        if(remapIter != payload.boneRemap.end()) {
                            return remapIter->second;
                        } else {
                            return 0;
                        }
                    };
                    vertex.boneIDs = glm::ivec4 {
                        remap(originalVertex.boneIDs[0]),
                        remap(originalVertex.boneIDs[1]),
                        remap(originalVertex.boneIDs[2]),
                        remap(originalVertex.boneIDs[3]),
                    };
                }
                vertexData = skinnedVerticesWithRemappedBoneIDs.data();
            } else {
                vertexData = primitive.vertices.data();
            }

            tinygltf::Buffer& geometryBuffer = isSkinned ? *pSkinnedGeometryBuffer : *pStaticGeometryBuffer;
            const int geometryBufferViewIndex = isSkinned ? skinnedVertexBufferViewIndex : staticVertexBufferViewIndex;

            auto makeVertexAttributeAccessor = [&](
                    std::string name,
                    int type,
                    int componentType,
                    std::size_t start
            ) {
                tinygltf::Accessor& accessor = payload.glTFModel.accessors.emplace_back();

                accessor.bufferView = geometryBufferViewIndex;
                accessor.byteOffset = start;
                accessor.count = vertexCount;
                accessor.name = std::move(name);
                accessor.type = type;
                accessor.componentType = componentType;

                return accessorIndex++;
            };

            tinygltf::Mesh& glTFMesh = model.meshes.emplace_back();
            glTFMesh.name = primitive.name;

            tinygltf::Primitive& glTFPrimitive = glTFMesh.primitives.emplace_back();
            glTFPrimitive.material = primitive.materialIndex;
            glTFPrimitive.mode = TINYGLTF_MODE_TRIANGLES;

            std::size_t indicesStartOffset = indicesBuffer.data.size();
            std::size_t indicesByteCount = primitive.indices.size() * sizeof(std::uint32_t);

            std::size_t geometryStartOffset = geometryBuffer.data.size();
            std::size_t geometryByteCount = vertexSize * vertexCount;

            indicesBuffer.data.resize(indicesBuffer.data.size() + indicesByteCount);
            memcpy(indicesBuffer.data.data() + indicesStartOffset, primitive.indices.data(), indicesByteCount);

            geometryBuffer.data.resize(geometryBuffer.data.size() + geometryByteCount);
            memcpy(geometryBuffer.data.data() + geometryStartOffset, vertexData, geometryByteCount);

            {
                tinygltf::Accessor& indicesAccessor = model.accessors.emplace_back();
                indicesAccessor.bufferView = indexBufferViewIndex;
                indicesAccessor.byteOffset = indicesStartOffset;
                indicesAccessor.count = primitive.indices.size();
                indicesAccessor.name = Carrot::sprintf("%s-indices", primitive.name.c_str());
                indicesAccessor.type = TINYGLTF_TYPE_SCALAR;
                indicesAccessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
                glTFPrimitive.indices = accessorIndex;
                accessorIndex++;
            }

            int positionAccessor = makeVertexAttributeAccessor(Carrot::sprintf("%s-positions", primitive.name.c_str()),
                                                               TINYGLTF_TYPE_VEC3, TINYGLTF_COMPONENT_TYPE_FLOAT,
                                                               offsetof(Carrot::SkinnedVertex, pos)+geometryStartOffset);
            glTFPrimitive.attributes["POSITION"] = positionAccessor;
            model.accessors[positionAccessor].minValues = vectorOfDoubles(primitive.minPos);
            model.accessors[positionAccessor].maxValues = vectorOfDoubles(primitive.maxPos);

            glTFPrimitive.attributes["NORMAL"] =
                    makeVertexAttributeAccessor(Carrot::sprintf("%s-normals", primitive.name.c_str()),
                                                TINYGLTF_TYPE_VEC3, TINYGLTF_COMPONENT_TYPE_FLOAT,
                                                offsetof(Carrot::SkinnedVertex, normal)+geometryStartOffset);
            glTFPrimitive.attributes["TEXCOORD_0"] =
                    makeVertexAttributeAccessor(Carrot::sprintf("%s-texCoords0", primitive.name.c_str()),
                                                TINYGLTF_TYPE_VEC2, TINYGLTF_COMPONENT_TYPE_FLOAT,
                                                offsetof(Carrot::SkinnedVertex, uv)+geometryStartOffset);
            glTFPrimitive.attributes["TANGENT"] =
                    makeVertexAttributeAccessor(Carrot::sprintf("%s-tangents", primitive.name.c_str()),
                                                TINYGLTF_TYPE_VEC4, TINYGLTF_COMPONENT_TYPE_FLOAT,
                                                offsetof(Carrot::SkinnedVertex, tangent)+geometryStartOffset);

            if(isSkinned) {
                glTFPrimitive.attributes["JOINTS_0"] =
                        makeVertexAttributeAccessor(Carrot::sprintf("%s-joints", primitive.name.c_str()),
                                                    TINYGLTF_TYPE_VEC4, TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE,
                                                    offsetof(Carrot::SkinnedVertex, boneIDs)+geometryStartOffset);

                glTFPrimitive.attributes["WEIGHTS_0"] =
                        makeVertexAttributeAccessor(Carrot::sprintf("%s-weights", primitive.name.c_str()),
                                                    TINYGLTF_TYPE_VEC4, TINYGLTF_COMPONENT_TYPE_FLOAT,
                                                    offsetof(Carrot::SkinnedVertex, boneWeights)+geometryStartOffset);
            }

            // write meshlets
            {
                auto& meshletsExtensionJSON = glTFPrimitive.extensions[Carrot::Render::GLTFLoader::CARROT_MESHLETS_EXTENSION_NAME];
                tinygltf::Value::Object meshletsExtension;

                auto& meshletBuffer = model.buffers[meshletBufferIndex];
                auto& clustersBuffer = model.buffers[clustersBufferIndex];

                int meshletsAccessorIndex = accessorIndex;
                accessorIndex++;

                int meshletVertexIndicesAccessorIndex = accessorIndex;
                accessorIndex++;

                int meshletIndicesAccessorIndex = accessorIndex;
                accessorIndex++;

                // meshlet data
                {
                    const std::size_t meshletCount = primitive.meshlets.size();
                    const std::size_t meshletStartIndex = meshletBuffer.data.size();
                    const std::size_t bufferSize = meshletCount * sizeof(Carrot::Render::Meshlet);
                    meshletBuffer.data.resize(meshletStartIndex + bufferSize);
                    memcpy(meshletBuffer.data.data() + meshletStartIndex, primitive.meshlets.data(), bufferSize);

                    tinygltf::Accessor& accessor = model.accessors.emplace_back();
                    accessor.bufferView = meshletBufferViewIndex;
                    accessor.byteOffset = meshletStartIndex;
                    accessor.count = bufferSize;
                    accessor.name = Carrot::sprintf("%s-meshlets", primitive.name.c_str());
                    accessor.type = TINYGLTF_TYPE_SCALAR;
                    accessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
                }

                // vertex indices
                {
                    const std::size_t count = primitive.meshletVertexIndices.size();
                    const std::size_t startIndex = clustersBuffer.data.size();
                    const std::size_t bufferSize = count * sizeof(std::uint32_t);
                    clustersBuffer.data.resize(startIndex + bufferSize);
                    memcpy(clustersBuffer.data.data() + startIndex, primitive.meshletVertexIndices.data(), bufferSize);

                    tinygltf::Accessor& accessor = model.accessors.emplace_back();
                    accessor.bufferView = clustersBufferViewIndex;
                    accessor.byteOffset = startIndex;
                    accessor.count = count;
                    accessor.name = Carrot::sprintf("%s-meshlets-vertex-indices", primitive.name.c_str());
                    accessor.type = TINYGLTF_TYPE_SCALAR;
                    accessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
                }

                // indices
                {
                    const std::size_t count = primitive.meshletIndices.size();
                    const std::size_t startIndex = clustersBuffer.data.size();
                    const std::size_t bufferSize = count * sizeof(std::uint32_t);
                    clustersBuffer.data.resize(startIndex + bufferSize);
                    memcpy(clustersBuffer.data.data() + startIndex, primitive.meshletIndices.data(), bufferSize);

                    tinygltf::Accessor& accessor = model.accessors.emplace_back();
                    accessor.bufferView = clustersBufferViewIndex;
                    accessor.byteOffset = startIndex;
                    accessor.count = count;
                    accessor.name = Carrot::sprintf("%s-meshlets-indices", primitive.name.c_str());
                    accessor.type = TINYGLTF_TYPE_SCALAR;
                    accessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
                }

                meshletsExtension["meshlets"] = tinygltf::Value { meshletsAccessorIndex };
                meshletsExtension["meshlets_vertex_indices"] = tinygltf::Value { meshletVertexIndicesAccessorIndex };
                meshletsExtension["meshlets_indices"] = tinygltf::Value { meshletIndicesAccessorIndex };
                meshletsExtensionJSON = tinygltf::Value{ std::move(meshletsExtension) };
            }
        }

        {
            auto& indicesBufferView = model.bufferViews[indexBufferViewIndex];
            indicesBufferView.name = "indexBuffer";
            indicesBufferView.byteLength = indicesBuffer.data.size();
            indicesBufferView.byteOffset = 0;
            indicesBufferView.buffer = indexBufferIndex;
        }
        {
            auto& meshletBuffer = model.buffers[meshletBufferIndex];
            auto& meshletsBufferView = model.bufferViews[meshletBufferViewIndex];
            meshletsBufferView.name = "Meshlets";
            meshletsBufferView.byteLength = meshletBuffer.data.size();
            meshletsBufferView.byteOffset = 0;
            meshletsBufferView.byteStride = sizeof(Carrot::Render::Meshlet);
            meshletsBufferView.buffer = meshletBufferIndex;
        }
        {
            auto& clustersBuffer = model.buffers[clustersBufferIndex];
            auto& clustersBufferView = model.bufferViews[clustersBufferViewIndex];
            clustersBufferView.name = "Cluster-data";
            clustersBufferView.byteLength = clustersBuffer.data.size();
            clustersBufferView.byteOffset = 0;
            clustersBufferView.buffer = clustersBufferIndex;
        }
        if(pStaticGeometryBuffer != nullptr)
        {
            auto& verticesBufferView = model.bufferViews[staticVertexBufferViewIndex];
            verticesBufferView.name = "StaticVertexBuffer";
            verticesBufferView.byteLength = pStaticGeometryBuffer->data.size();
            verticesBufferView.byteOffset = 0;
            verticesBufferView.buffer = staticVertexBufferIndex;
            verticesBufferView.byteStride = sizeof(Carrot::Vertex);
        }
        if(pSkinnedGeometryBuffer != nullptr)
        {
            auto& verticesBufferView = model.bufferViews[skinnedVertexBufferViewIndex];
            verticesBufferView.name = "SkinnedVertexBuffer";
            verticesBufferView.byteLength = pSkinnedGeometryBuffer->data.size();
            verticesBufferView.byteOffset = 0;
            verticesBufferView.buffer = skinnedVertexBufferIndex;
            verticesBufferView.byteStride = sizeof(Carrot::SkinnedVertex);
        }
    }

    /**
     * Used to avoid writing default transforms in final glTF
     */
    static bool isDefaultTransform(const glm::mat4& transform) {
        return glm::identity<glm::mat4>() == transform;
    }

    static void writeNodes(Payload& payload, const Carrot::Render::LoadedScene& scene) {
        if(!scene.nodeHierarchy) {
            return;
        }
        // start by creating children to nodes that have multiple meshes (which is not supported by glTF)
        Carrot::Render::Skeleton& newHierarchy = payload.hierarchy;
        newHierarchy = *scene.nodeHierarchy;
        std::function<void(Carrot::Render::SkeletonTreeNode&)> fission = [&](Carrot::Render::SkeletonTreeNode& node) {
            // children-first to avoid doing unnecessary checks for newly created children
            for(auto& child : node.getChildren()) {
                fission(child);
            }

            if(node.meshIndices.has_value()) {
                std::size_t meshCount = node.meshIndices.value().size();
                for(std::size_t meshIndex = 1; meshIndex < meshCount; meshIndex++) {
                    auto& newChild = node.newChild();
                    newChild.bone.name = Carrot::sprintf("%s-mesh%llu", node.bone.name.c_str(), meshIndex);
                    newChild.meshIndices = std::vector<std::size_t>{};
                    newChild.meshIndices.value().push_back(node.meshIndices.value()[meshIndex]);
                }

                node.meshIndices.value().resize(1);
            }
        };

        fission(newHierarchy.hierarchy);
        newHierarchy.hierarchy.bone.transform = carrotSpaceToGLTFSpace * newHierarchy.hierarchy.bone.transform;
        newHierarchy.hierarchy.bone.originalTransform = carrotSpaceToGLTFSpace * newHierarchy.hierarchy.bone.originalTransform;

        // actually export new hierarchy
        std::size_t nodeIndex = 0;
        tinygltf::Model& model = payload.glTFModel;
        std::function<void(const Carrot::Render::SkeletonTreeNode&)> exportNode = [&](const Carrot::Render::SkeletonTreeNode& node) {
            std::size_t currentNodeIndex = nodeIndex++;
            const glm::mat4& nodeTransform = node.bone.transform;
            payload.nodeMap[&node] = currentNodeIndex;
            model.nodes.emplace_back(); // cannot keep a reference, will be invalidated by recursive exportNode calls

#define currentNode (model.nodes[currentNodeIndex])
            currentNode.name = node.bone.name;

            if(!isDefaultTransform(nodeTransform)) {
                glm::vec3 translation;
                glm::quat rotation;
                glm::vec3 scale;
                glm::vec3 skew;
                glm::vec4 perspective;
                glm::decompose(nodeTransform, scale, rotation, translation, skew,perspective);

                currentNode.translation = vectorOfDoubles(translation);
                currentNode.scale = vectorOfDoubles(scale);
                currentNode.rotation = vectorOfDoubles(rotation);
            }

            if(node.meshIndices.has_value()) {
                verify(node.meshIndices.value().size() <= 1, Carrot::sprintf("Programming error: node '%s' with multiple meshes was not split into multiple subnodes.", node.bone.name.c_str()));
                if(node.meshIndices.value().size() == 1) {
                    int meshIndex = node.meshIndices.value()[0];
                    auto& primitive = scene.primitives[meshIndex];
                    if(primitive.isSkinned) {
                        currentNode.mesh = meshIndex;
                        currentNode.skin = 0;
                    } else {
                        currentNode.mesh = meshIndex;
                    }
                }
            }

            currentNode.children.reserve(node.getChildren().size());
            for(const auto& child : node.getChildren()) {
                std::size_t childID = nodeIndex;
                currentNode.children.emplace_back(childID);
                exportNode(child);
            }

#undef currentNode
        };
        exportNode(newHierarchy.hierarchy);
        payload.exportedNodes = nodeIndex != 0;
    }

    static void writeScenes(Payload& payload, const Carrot::Render::LoadedScene& scene) {
        // multiple scenes are merged into a single one by our glTF loader -> we can export only a single scene
        auto& model = payload.glTFModel;
        model.defaultScene = 0;
        auto& glTFScene = model.scenes.emplace_back();

        if(payload.exportedNodes) {
            glTFScene.nodes.push_back(0);
        }
    }

    static void writeSkins(const std::string& modelName, Payload& payload, const Carrot::Render::LoadedScene& scene) {
        if(scene.boneMapping.empty()) {
            return;
        }

        auto& model = payload.glTFModel;
        // Because Carrot supports a single hierarchy per model, there can only be a single skin per produced glTF file
        const auto& [meshIndex, boneMapping] = *scene.boneMapping.begin();
        const auto& inverseBindMatrices = scene.offsetMatrices.at(meshIndex);
        tinygltf::Skin& skin = model.skins.emplace_back();
        skin.name = Carrot::sprintf("Skin for mesh %d", meshIndex);

        std::size_t inverseBindMatricesBufferIndex = model.buffers.size();
        auto& inverseBindMatricesBuffer = model.buffers.emplace_back();
        inverseBindMatricesBuffer.uri = modelName + "-inverse-bind-matrices.bin";
        inverseBindMatricesBuffer.name = "Inverse bind matrices";

        std::function<void(const Carrot::Render::SkeletonTreeNode&)> recurseJoints = [&](const Carrot::Render::SkeletonTreeNode& skeletonTreeNode) {
            const std::string& boneName = skeletonTreeNode.bone.name;
            auto nodeIDIter = payload.nodeMap.find(&skeletonTreeNode);
            if(nodeIDIter != payload.nodeMap.end()) { // scene root is not inside this map
                auto iter = boneMapping.find(boneName);
                if(iter != boneMapping.end()) { // check whether this node is used as a joint/bone
                    std::size_t nodeID = nodeIDIter->second;
                    payload.boneRemap[nodeID] = skin.joints.size();
                    payload.reverseBoneRemap[skin.joints.size()] = nodeID;
                    skin.joints.emplace_back(nodeID);

                    std::size_t inverseBindMatrixLocation = inverseBindMatricesBuffer.data.size();
                    inverseBindMatricesBuffer.data.resize(inverseBindMatricesBuffer.data.size() + sizeof(float[4*4]));
                    float* matrix = reinterpret_cast<float*>(&inverseBindMatricesBuffer.data[inverseBindMatrixLocation]);

                    const glm::mat4& inverseBindMatrix = inverseBindMatrices.at(boneName);
                    for(int y = 0; y < 4; y++) {
                        for(int x = 0; x < 4; x++) {
                            matrix[x + y * 4] = inverseBindMatrix[y][x];
                        }
                    }
                }
            }
            for(const auto& child : skeletonTreeNode.getChildren()) {
                recurseJoints(child);
            }
        };
        recurseJoints(payload.hierarchy.hierarchy);

        int inverseBindMatricesBufferViewIndex = model.bufferViews.size();
        auto& inverseBindMatricesBufferView = model.bufferViews.emplace_back();
        inverseBindMatricesBufferView.buffer = (int)inverseBindMatricesBufferIndex;

        inverseBindMatricesBufferView.name = "Inverse bind matrices";
        inverseBindMatricesBufferView.byteLength = inverseBindMatricesBuffer.data.size();
        inverseBindMatricesBufferView.byteOffset = 0;

        std::size_t inverseBindMatricesAccessorIndex = model.accessors.size();
        auto& inverseBindMatricesAccessor = model.accessors.emplace_back();
        inverseBindMatricesAccessor.bufferView = (int)inverseBindMatricesBufferViewIndex;
        inverseBindMatricesAccessor.byteOffset = 0;
        inverseBindMatricesAccessor.count = inverseBindMatricesBuffer.data.size() / sizeof(glm::mat4);
        inverseBindMatricesAccessor.name = "Inverse bind matrices accessor";
        inverseBindMatricesAccessor.type = TINYGLTF_TYPE_MAT4;
        inverseBindMatricesAccessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;

        skin.inverseBindMatrices = (int)inverseBindMatricesAccessorIndex;
    }

    static void writeAnimations(const std::string& modelName, Payload& payload, const Carrot::Render::LoadedScene& scene) {
        auto& model = payload.glTFModel;

        if(scene.boneMapping.empty()) {
            return;
        }

        // Because Carrot supports a single hierarchy per model, there can only be a single skin per produced glTF file
        const auto& [meshIndex, boneMapping] = *scene.boneMapping.begin();
        const auto& inverseBindMatrices = scene.offsetMatrices.at(meshIndex);

        int animationBufferIndex = model.buffers.size();
        auto& animationBuffer = model.buffers.emplace_back();
        animationBuffer.name = "Animation Data";
        animationBuffer.uri = modelName + "-animation-data.bin";
        auto& animationData = animationBuffer.data;

        int animationBufferViewIndex = model.bufferViews.size();
        auto& animationBufferView = model.bufferViews.emplace_back();
        animationBufferView.buffer = animationBufferIndex;
        animationBufferView.name = "Animation data";

        std::unordered_map<std::size_t, const Carrot::Render::SkeletonTreeNode*> reverseBoneMapping;
        //std::unordered_map<const Carrot::Render::SkeletonTreeNode*, std::size_t> completeBoneMapping;
        std::unordered_map<std::string, std::size_t> completeBoneMapping;
        for(const auto& [k, v] : payload.nodeMap) {
            reverseBoneMapping[v] = k;
            completeBoneMapping[k->bone.name] = v;
        }

        for(const auto& [animationName, animationIndex] : scene.animationMapping) {
            const auto& animation = scene.animationData[animationIndex];
            auto& glTFAnimation = model.animations.emplace_back();
            glTFAnimation.name = animationName;

            // TODO: deduplicate timestamp data
            // write timestamp data to animation buffer
            std::size_t timestampDataOffset = animationData.size();
            animationData.resize(animationData.size() + sizeof(float) * animation.keyframeCount);
            float* pTimestamps = reinterpret_cast<float*>(&animationData[timestampDataOffset]);
            for (int i = 0; i < animation.keyframeCount; ++i) {
                pTimestamps[i] = animation.keyframes[i].timestamp;
            }

            int timestampsBufferViewIndex = model.bufferViews.size();
            auto& timestampsBufferView = model.bufferViews.emplace_back();
            timestampsBufferView.buffer = animationBufferIndex;
            timestampsBufferView.name = Carrot::sprintf("Animation '%s' Timestamps", animationName.c_str());
            timestampsBufferView.byteOffset = timestampDataOffset;
            timestampsBufferView.byteLength = sizeof(float) * animation.keyframeCount;

            int timestampsAccessorIndex = model.accessors.size();
            auto& timestampsAccessor = model.accessors.emplace_back();
            timestampsAccessor.bufferView = timestampsBufferViewIndex;
            timestampsAccessor.count = animation.keyframeCount;
            timestampsAccessor.type = TINYGLTF_TYPE_SCALAR;
            timestampsAccessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
            timestampsAccessor.name = Carrot::sprintf("Animation '%s' Timestamps", animationName.c_str());
            timestampsAccessor.minValues = { animation.keyframes[0].timestamp };
            timestampsAccessor.maxValues = { animation.keyframes[animation.keyframeCount - 1].timestamp };

            struct TRS {
                glm::vec3 translation{0.0f};
                glm::quat rotation = glm::identity<glm::quat>();
                glm::vec3 scale{1.0f};
            };
            std::unordered_map<std::size_t, std::vector<TRS>> trsKeyframesPerBone;
            std::size_t boneCount = animation.keyframes[0].boneTransforms.size();
            for(std::size_t boneIndex = 0; boneIndex < boneCount; boneIndex++) {
                auto& trsKeyframesForThisBone = trsKeyframesPerBone[boneIndex];
                trsKeyframesForThisBone.resize(animation.keyframeCount);

                bool hasTranslation = false;
                bool hasRotation = false;
                bool hasScale = false;

                auto nodeIndexIter = payload.reverseBoneRemap.find(boneIndex);
                if(nodeIndexIter == payload.reverseBoneRemap.end()) {
                    continue;
                }
                const auto& nodeIndex = nodeIndexIter->second;
                const auto* pCurrentBone = reverseBoneMapping.at(nodeIndex);

                glm::mat4 inverseBindMatrix{ 1.0f };
                auto inverseBindMatrixIter = inverseBindMatrices.find(pCurrentBone->bone.name);
                if(inverseBindMatrixIter != inverseBindMatrices.end()) {
                    inverseBindMatrix = inverseBindMatrixIter->second;
                }
                const glm::mat4 invBoneOffset = glm::inverse(inverseBindMatrix);

                for(std::size_t keyframeIndex = 0; keyframeIndex < animation.keyframeCount; keyframeIndex++) {
                    const auto& keyframe = animation.keyframes[keyframeIndex];
                    const auto& transform = keyframe.boneTransforms[boneIndex];
                    auto& trs = trsKeyframesForThisBone[keyframeIndex];

                    glm::mat4 invParent {1.0f};
                    if(pCurrentBone->pParent != nullptr) {
                        auto pParentIter = completeBoneMapping.find(pCurrentBone->pParent->bone.name);
                        if(pParentIter != completeBoneMapping.end()) {
                            std::size_t parentID = pParentIter->second;

                            auto parentBoneIDIter = payload.boneRemap.find(parentID);
                            if(parentBoneIDIter != payload.boneRemap.end()) {
                                const int parentBoneID = parentBoneIDIter->second;

                                glm::mat4 parentInverseBindMatrix{ 1.0f };
                                auto parentInverseBindMatrixIter = inverseBindMatrices.find(pCurrentBone->pParent->bone.name);
                                if(parentInverseBindMatrixIter != inverseBindMatrices.end()) {
                                    parentInverseBindMatrix = glm::inverse(parentInverseBindMatrixIter->second);
                                }
                                invParent = glm::inverse(carrotSpaceToGLTFSpace * keyframe.boneTransforms[parentBoneID] * parentInverseBindMatrix);
                            }
                        }
                    }

                    glm::vec3 skew;
                    glm::vec4 perspective;
                    glm::decompose(invParent * carrotSpaceToGLTFSpace * transform * invBoneOffset, trs.scale, trs.rotation, trs.translation, skew,perspective);

                    hasTranslation |= glm::any(glm::epsilonNotEqual(trs.translation, glm::vec3{0.0f}, 10e-6f));
                    hasRotation |= glm::any(glm::epsilonNotEqual(trs.rotation, glm::identity<glm::quat>(), 10e-6f));
                    hasScale |= glm::any(glm::epsilonNotEqual(trs.scale, glm::vec3{1.0f}, 10e-6f));
                }

                // translation
                if(hasTranslation) {
                    std::size_t dataOffset = animationData.size();

                    // copy data
                    animationData.resize(dataOffset + animation.keyframeCount * sizeof(glm::vec3));
                    glm::vec3* pTranslations = reinterpret_cast<glm::vec3*>(&animationData[dataOffset]);
                    for(std::size_t keyframeIndex = 0; keyframeIndex < animation.keyframeCount; keyframeIndex++) {
                        pTranslations[keyframeIndex] = trsKeyframesForThisBone[keyframeIndex].translation;
                    }

                    // prepare accessor
                    int translationDataAccessorIndex = model.accessors.size();
                    auto& translationDataAccessor = model.accessors.emplace_back();
                    translationDataAccessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
                    translationDataAccessor.type = TINYGLTF_TYPE_VEC3;
                    translationDataAccessor.count = animation.keyframeCount;
                    translationDataAccessor.name = Carrot::sprintf("Translation data for %s bone %d", animationName.c_str(), boneIndex);
                    translationDataAccessor.byteOffset = dataOffset;
                    translationDataAccessor.bufferView = animationBufferViewIndex;

                    // create sampler & channel
                    int samplerIndex = glTFAnimation.samplers.size();
                    auto& sampler = glTFAnimation.samplers.emplace_back();
                    sampler.input = timestampsAccessorIndex;
                    sampler.output = translationDataAccessorIndex;

                    auto& channel = glTFAnimation.channels.emplace_back();
                    channel.target_path = "translation";
                    channel.target_node = nodeIndex;
                    channel.sampler = samplerIndex;
                }

                // rotation
                if(hasRotation) {
                    std::size_t dataOffset = animationData.size();

                    // copy data
                    animationData.resize(dataOffset + animation.keyframeCount * sizeof(glm::quat));
                    glm::quat* pRotations = reinterpret_cast<glm::quat*>(&animationData[dataOffset]);
                    for(std::size_t keyframeIndex = 0; keyframeIndex < animation.keyframeCount; keyframeIndex++) {
                        const auto& rot = trsKeyframesForThisBone[keyframeIndex].rotation;
                        pRotations[keyframeIndex] = rot;
                    }

                    // prepare accessor
                    int dataAccessorIndex = model.accessors.size();
                    auto& dataAccessor = model.accessors.emplace_back();
                    dataAccessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
                    dataAccessor.type = TINYGLTF_TYPE_VEC4;
                    dataAccessor.count = animation.keyframeCount;
                    dataAccessor.name = Carrot::sprintf("Rotation data for %s bone %d", animationName.c_str(), boneIndex);
                    dataAccessor.byteOffset = dataOffset;
                    dataAccessor.bufferView = animationBufferViewIndex;

                    // create sampler & channel
                    int samplerIndex = glTFAnimation.samplers.size();
                    auto& sampler = glTFAnimation.samplers.emplace_back();
                    sampler.input = timestampsAccessorIndex;
                    sampler.output = dataAccessorIndex;

                    auto& channel = glTFAnimation.channels.emplace_back();
                    channel.target_path = "rotation";
                    channel.target_node = nodeIndex;
                    channel.sampler = samplerIndex;
                }

                // scale
                if(hasScale) {
                    std::size_t dataOffset = animationData.size();

                    // copy data
                    animationData.resize(dataOffset + animation.keyframeCount * sizeof(glm::vec3));
                    glm::vec3* pScales = reinterpret_cast<glm::vec3*>(&animationData[dataOffset]);
                    for(std::size_t keyframeIndex = 0; keyframeIndex < animation.keyframeCount; keyframeIndex++) {
                        pScales[keyframeIndex] = trsKeyframesForThisBone[keyframeIndex].scale;
                    }

                    // prepare accessor
                    int dataAccessorIndex = model.accessors.size();
                    auto& dataAccessor = model.accessors.emplace_back();
                    dataAccessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
                    dataAccessor.type = TINYGLTF_TYPE_VEC3;
                    dataAccessor.count = animation.keyframeCount;
                    dataAccessor.name = Carrot::sprintf("Scale data for %s bone %d", animationName.c_str(), boneIndex);
                    dataAccessor.byteOffset = dataOffset;
                    dataAccessor.bufferView = animationBufferViewIndex;

                    // create sampler & channel
                    int samplerIndex = glTFAnimation.samplers.size();
                    auto& sampler = glTFAnimation.samplers.emplace_back();
                    sampler.input = timestampsAccessorIndex;
                    sampler.output = dataAccessorIndex;

                    auto& channel = glTFAnimation.channels.emplace_back();
                    channel.target_path = "scale";
                    channel.target_node = nodeIndex;
                    channel.sampler = samplerIndex;
                }
            }
        }

        model.bufferViews[animationBufferViewIndex].byteLength = animationData.size();
    }

    tinygltf::Model writeAsGLTF(const std::string& modelName, const Carrot::Render::LoadedScene& scene) {
        tinygltf::Model model;

        model.asset.generator = "Fertilizer v1";
        model.asset.version = "2.0";
        model.extensionsUsed.emplace_back(Carrot::Render::GLTFLoader::CARROT_MESHLETS_EXTENSION_NAME);

        Payload payload {
                .glTFModel = model,
        };

        writeTextures(payload, scene);
        writeMaterials(payload, scene);
        writeNodes(payload, scene);
        writeScenes(payload, scene);
        writeSkins(modelName, payload, scene); // needs to be after writeNodes to know the node IDs of joints
        writeMeshes(modelName, payload, scene); // needs to be after writeSkins to know the joint IDs
        writeAnimations(modelName, payload, scene);

        return std::move(model);
    }
}