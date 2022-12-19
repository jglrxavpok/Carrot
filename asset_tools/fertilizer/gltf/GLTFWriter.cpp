//
// Created by jglrxavpok on 17/12/2022.
//

#include "GLTFWriter.h"
#include "core/utils/stringmanip.h"
#include "glm/ext/matrix_transform.hpp"

namespace Fertilizer {
    struct Payload {
        tinygltf::Model& glTFModel;

        bool exportedNodes = false;
        std::unordered_map<Carrot::IO::VFS::Path, int> textureIndices;
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
        auto it = payload.textureIndices.find(texturePath);
        verify(it != payload.textureIndices.end(), "Programming error: a texture was referenced by scene, but not written to Payload");

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

    static void writeMeshes(Payload& payload, const Carrot::Render::LoadedScene& scene) {
        tinygltf::Model& model = payload.glTFModel;
        model.meshes.reserve(scene.primitives.size());

        model.buffers.emplace_back();
        model.buffers.emplace_back();
        tinygltf::Buffer& indicesBuffer = model.buffers[0];
        tinygltf::Buffer& geometryBuffer = model.buffers[1];
        indicesBuffer.name = "IndexBuffer";
        geometryBuffer.name = "VertexBuffer";

        std::uint32_t accessorIndex = 0;
        for(const auto& primitive : scene.primitives) {
            auto makeVertexAttributeAccessor = [&](
                    std::string name,
                    int type,
                    int componentType,
                    std::size_t start
            ) {
                tinygltf::Accessor& accessor = payload.glTFModel.accessors.emplace_back();

                accessor.bufferView = 1;
                accessor.byteOffset = start;
                accessor.count = primitive.vertices.size();
                accessor.name = std::move(name);
                accessor.type = type;
                accessor.componentType = componentType;

                return accessorIndex++;
            };

            // TODO: our glTF loader does not support skinned meshes yet
            tinygltf::Mesh& glTFMesh = model.meshes.emplace_back();
            tinygltf::Primitive& glTFPrimitive = glTFMesh.primitives.emplace_back();
            glTFPrimitive.material = primitive.materialIndex;
            glTFPrimitive.mode = TINYGLTF_MODE_TRIANGLES;

            std::size_t indicesStartOffset = indicesBuffer.data.size();
            std::size_t indicesByteCount = primitive.indices.size() * sizeof(std::uint32_t);

            std::size_t geometryStartOffset = geometryBuffer.data.size();
            std::size_t geometryByteCount = sizeof(Carrot::Vertex) * primitive.vertices.size();

            indicesBuffer.data.resize(indicesBuffer.data.size() + indicesByteCount);
            memcpy(indicesBuffer.data.data() + indicesStartOffset, primitive.indices.data(), indicesByteCount);

            geometryBuffer.data.resize(geometryBuffer.data.size() + geometryByteCount);
            memcpy(geometryBuffer.data.data() + geometryStartOffset, primitive.vertices.data(), geometryByteCount);

            {
                tinygltf::Accessor& indicesAccessor = model.accessors.emplace_back();
                indicesAccessor.bufferView = 0;
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
                                                               offsetof(Carrot::Vertex, pos));
            glTFPrimitive.attributes["POSITION"] = positionAccessor;
            model.accessors[positionAccessor].minValues = vectorOfDoubles(primitive.minPos);
            model.accessors[positionAccessor].maxValues = vectorOfDoubles(primitive.maxPos);

            glTFPrimitive.attributes["NORMAL"] =
                    makeVertexAttributeAccessor(Carrot::sprintf("%s-normals", primitive.name.c_str()),
                                                TINYGLTF_TYPE_VEC3, TINYGLTF_COMPONENT_TYPE_FLOAT,
                                                offsetof(Carrot::Vertex, normal));
            glTFPrimitive.attributes["TEXCOORD_0"] =
                    makeVertexAttributeAccessor(Carrot::sprintf("%s-texCoords0", primitive.name.c_str()),
                                                TINYGLTF_TYPE_VEC2, TINYGLTF_COMPONENT_TYPE_FLOAT,
                                                offsetof(Carrot::Vertex, uv));
            glTFPrimitive.attributes["TANGENT"] =
                    makeVertexAttributeAccessor(Carrot::sprintf("%s-tangents", primitive.name.c_str()),
                                                TINYGLTF_TYPE_VEC4, TINYGLTF_COMPONENT_TYPE_FLOAT,
                                                offsetof(Carrot::Vertex, tangent)); // TODO: VEC4 tangents
        }

        {
            auto& indicesBufferView = model.bufferViews.emplace_back();
            indicesBufferView.name = "indexBuffer";
            indicesBufferView.byteLength = indicesBuffer.data.size();
            indicesBufferView.byteOffset = 0;
            indicesBufferView.buffer = 0;
        }
        {
            auto& verticesBufferView = model.bufferViews.emplace_back();
            verticesBufferView.name = "vertexBuffer";
            verticesBufferView.byteLength = geometryBuffer.data.size();
            verticesBufferView.byteOffset = 0;
            verticesBufferView.buffer = 1;
            verticesBufferView.byteStride = sizeof(Carrot::Vertex);
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
        Carrot::Render::Skeleton newHierarchy = *scene.nodeHierarchy;
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
                    newChild.meshIndices.value().push_back(meshIndex);
                }

                node.meshIndices.value().resize(1);
            }
        };

        fission(newHierarchy.hierarchy);

        // actually export new hierarchy
        std::size_t nodeIndex = 0;
        tinygltf::Model& model = payload.glTFModel;
        std::function<void(const Carrot::Render::SkeletonTreeNode&)> exportNode = [&](const Carrot::Render::SkeletonTreeNode& node) {
            // check if we can skip this node
            bool skippable = false;
            skippable = node.getChildren().size() <= 1
                    && (!node.meshIndices.has_value() || node.meshIndices.value().empty())
                    && isDefaultTransform(node.bone.originalTransform);

            if(skippable) {
                if(!node.getChildren().empty()) {
                    exportNode(node.getChildren().front());
                }
                return;
            }

            std::size_t currentNodeIndex = nodeIndex++;
            model.nodes.emplace_back(); // cannot keep a reference, will be invalidated by recursive exportNode calls

#define currentNode (model.nodes[currentNodeIndex])
            currentNode.name = node.bone.name;

            if(!isDefaultTransform(node.bone.originalTransform)) {
                currentNode.matrix.resize(4*4);
                for (int x = 0; x < 4; ++x) {
                    for (int y = 0; y < 4; ++y) {
                        currentNode.matrix[y + x * 4] = node.bone.originalTransform[x][y];
                    }
                }
            }

            if(node.meshIndices.has_value()) {
                verify(node.meshIndices.value().size() <= 1, Carrot::sprintf("Programming error: node '%s' with multiple meshes was not split into multiple subnodes.", node.bone.name.c_str()));
                if(node.meshIndices.value().size() == 1) {
                    currentNode.mesh = node.meshIndices.value()[0];
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

    tinygltf::Model writeAsGLTF(const Carrot::Render::LoadedScene& scene) {
        tinygltf::Model model;

        model.asset.generator = "Fertilizer v1";
        model.asset.version = "2.0";

        Payload payload {
                .glTFModel = model,
        };

        writeTextures(payload, scene);
        writeMaterials(payload, scene);
        writeMeshes(payload, scene);
        writeNodes(payload, scene);
        writeScenes(payload, scene);

        return std::move(model);
    }
}