//
// Created by jglrxavpok on 27/10/2022.
//

#include "GLTFLoader.h"
#include "core/io/Logging.hpp"
#include <core/utils/Profiling.h>
#include <glm/gtx/quaternion.hpp>

#include "core/io/vfs/VirtualFileSystem.h"

#pragma optimize("",off)

namespace Carrot::Render {

    constexpr const char* const KHR_TEXTURE_BASISU_EXTENSION_NAME = "KHR_texture_basisu";

    constexpr const char* const SUPPORTED_EXTENSIONS[] = {
            KHR_TEXTURE_BASISU_EXTENSION_NAME,
    };

    struct PrimitiveInformation {
        bool hasTangents = false;
        bool hasTexCoords = false;
        bool hasNormals = false;
    };

    bool gltfReadWholeFile(std::vector<unsigned char>* out,
                           std::string* err, const std::string& filepath,
                           void* userData) {
        ZoneScoped;
        IO::Resource resource { IO::VFS::Path { filepath } };
        std::uint64_t resourceSize = resource.getSize();
        out->resize(resourceSize);
        resource.read(out->data(), resourceSize);
        return true;
    }

    static std::string decodeURI(const std::string& uri) {
        ZoneScoped;
        std::string decoded;
        decoded.reserve(uri.size());

        auto charToHex = [](char c) {
            if(c >= '0' && c <= '9') {
                return c - '0';
            }
            if(c >= 'A' && c <= 'F') {
                return c - 'A' + 10;
            }
            if(c >= 'a' && c <= 'f') {
                return c - 'a' + 10;
            }
            return 0;
        };

        for (std::size_t i = 0; i < uri.size(); ) {
            char c = uri[i];
            if(c == '%') {
                verify(uri.size() > i + 2, "unexpected end of string");
                int characterCode = charToHex(uri[i+1]) * 16 + charToHex(uri[i+2]);
                decoded.append(1, (char)characterCode);
                i += 3;
            } else {
                decoded.append(1, c);
                i++;
            }
        }

        return decoded;
    }

    std::string gltfExpandFilePath(const std::string& filepath, void* userData) {
        return decodeURI(filepath);
    }

    bool gltfFileExists(const std::string& abs_filename, void* userData) {
        ZoneScoped;
        const IO::Resource& modelResource = *(static_cast<const IO::Resource*>(userData));
        IO::VFS::Path p { modelResource.getName() };
        return GetVFS().exists(p);
    }

    static glm::vec3 toVec3(std::span<const double> doubles, const glm::vec3& defaultValue) {
        if(doubles.empty())
            return defaultValue;
        verify(doubles.size() == 3, "Array must have exactly 3 values");
        return glm::vec3 {
                static_cast<float>(doubles[0]),
                static_cast<float>(doubles[1]),
                static_cast<float>(doubles[2]),
        };
    }

    static glm::vec4 toVec4(std::span<const double> doubles, const glm::vec4& defaultValue) {
        if(doubles.empty())
            return defaultValue;
        verify(doubles.size() == 4, "Array must have exactly 4 values");
        return glm::vec4 {
                static_cast<float>(doubles[0]),
                static_cast<float>(doubles[1]),
                static_cast<float>(doubles[2]),
                static_cast<float>(doubles[3]),
        };
    }

    static glm::quat toQuat(std::span<const double> doubles) {
        if(doubles.empty())
            return glm::identity<glm::quat>();
        verify(doubles.size() == 4, "Array must have exactly 4 values");
        return glm::quat {
                static_cast<float>(doubles[3]), // w first for GLM
                static_cast<float>(doubles[0]),
                static_cast<float>(doubles[1]),
                static_cast<float>(doubles[2]),
        };
    }

    /**
     * Compute data stride based on buffer view stride + accessor component size + accessor type
     */
    static std::size_t computeStride(const tinygltf::BufferView& bufferView, const tinygltf::Accessor& accessor) {
        ZoneScoped;
        if(bufferView.byteStride != 0)
            return bufferView.byteStride;

        int typeMultiplier = 0;

        switch(accessor.type) {
            case TINYGLTF_TYPE_SCALAR:
                typeMultiplier = 1;
                break;

            case TINYGLTF_TYPE_MAT2:
                typeMultiplier = 4;
                break;

            case TINYGLTF_TYPE_MAT3:
                typeMultiplier = 9;
                break;

            case TINYGLTF_TYPE_MAT4:
                typeMultiplier = 16;
                break;

            case TINYGLTF_TYPE_VEC2:
                typeMultiplier = 2;
                break;

            case TINYGLTF_TYPE_VEC3:
                typeMultiplier = 3;
                break;

            case TINYGLTF_TYPE_VEC4:
                typeMultiplier = 4;
                break;
        }

        verify(typeMultiplier != 0, Carrot::sprintf("Missing switch case for value %d", accessor.type));

        switch(accessor.componentType) {
            case TINYGLTF_COMPONENT_TYPE_BYTE:
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                return typeMultiplier * 1;

            case TINYGLTF_COMPONENT_TYPE_SHORT:
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                return typeMultiplier * 2;

            case TINYGLTF_COMPONENT_TYPE_INT:
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
            case TINYGLTF_COMPONENT_TYPE_FLOAT:
                return typeMultiplier * 4;

            case TINYGLTF_COMPONENT_TYPE_DOUBLE:
                return typeMultiplier * 8;
        }
        verify(false, Carrot::sprintf("missing switch case for value %d", accessor.componentType));
        return -1;
    }

    template<typename T>
    static T readFromAccessor(std::size_t index, const tinygltf::Accessor& accessor, const tinygltf::Model& model) {
        ZoneScoped;
        const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer& sourceBuffer = model.buffers[bufferView.buffer];
        const std::size_t stride = computeStride(bufferView, accessor);
        const void* pSource = sourceBuffer.data.data() + bufferView.byteOffset + accessor.byteOffset + index * stride;

        return *((const T*)pSource);
    }

    static void loadVertices(LoadedPrimitive& loadedPrimitive, const tinygltf::Model& model, const tinygltf::Primitive& primitive, PrimitiveInformation& info) {
        ZoneScoped;
        std::vector<Vertex>& vertices = loadedPrimitive.vertices;
        const tinygltf::Accessor& positionsAccessor = model.accessors[primitive.attributes.at("POSITION")];
        const tinygltf::Accessor& normalsAccessor = model.accessors[primitive.attributes.at("NORMAL")];
        info.hasNormals = true;

        int texCoordsAccessorIndex = -1;
        auto texCoordsAttributeIt = primitive.attributes.find("TEXCOORD_0");
        if(texCoordsAttributeIt != primitive.attributes.end()) {
            texCoordsAccessorIndex = texCoordsAttributeIt->second;
            info.hasTexCoords = true;
        }

        int colorAccessorIndex = -1;
        auto colorAttributeIt = primitive.attributes.find("COLOR_0");
        if(colorAttributeIt != primitive.attributes.end()) {
            colorAccessorIndex = colorAttributeIt->second;
        }

        int tangentAccessorIndex = -1;
        auto tangentAttributeIt = primitive.attributes.find("TANGENT");
        if(tangentAttributeIt != primitive.attributes.end()) {
            tangentAccessorIndex = tangentAttributeIt->second;
            info.hasTangents = true;
        }

        const tinygltf::Accessor* vertexColorAccessor = colorAccessorIndex != -1 ? &model.accessors[colorAccessorIndex] : nullptr;
        const tinygltf::Accessor* texCoordsAccessor = texCoordsAccessorIndex != -1 ? &model.accessors[texCoordsAccessorIndex] : nullptr;
        const tinygltf::Accessor* tangentsAccessor = tangentAccessorIndex != -1 ? &model.accessors[tangentAccessorIndex] : nullptr;
        // TODO: UV2
        // TODO: Joints
        // TODO: Weights
        // TODO: support skinned meshes
        // TODO: support compressed meshes

        verify(positionsAccessor.count == normalsAccessor.count, "Mismatched position/normal count");

        if(texCoordsAccessor != nullptr) {
            verify(positionsAccessor.count == texCoordsAccessor->count, "Mismatched position/texCoords count");
        }

        if(tangentsAccessor != nullptr) {
            verify(positionsAccessor.count == tangentsAccessor->count, "Mismatched position/tangents count");
        }

        loadedPrimitive.minPos = toVec3(positionsAccessor.minValues, glm::vec3{INFINITY});
        loadedPrimitive.maxPos = toVec3(positionsAccessor.maxValues, glm::vec3{-INFINITY});
        vertices.resize(positionsAccessor.count);
        for (std::size_t i = 0; i < positionsAccessor.count; ++i) {
            Carrot::Vertex& vertex = vertices[i];
            glm::vec3 pos = readFromAccessor<glm::vec3>(i, positionsAccessor, model);
            vertex.pos = glm::vec4 { pos, 1.0f };

            vertex.normal = readFromAccessor<glm::vec3>(i, normalsAccessor, model);
            if(tangentsAccessor) {
                vertex.tangent = readFromAccessor<glm::vec4>(i, *tangentsAccessor, model);
            } else {
                vertex.tangent = glm::vec4(0.0, 0.0, 0.0, 1.0);
            }

            if(texCoordsAccessor) {
                vertex.uv = readFromAccessor<glm::vec2>(i, *texCoordsAccessor, model);
            } else {
                vertex.uv = glm::vec2(0.0f);
            }

            if(vertexColorAccessor) {
                vertex.color = readFromAccessor<glm::vec4>(i, *vertexColorAccessor, model);
            } else {
                vertex.color = glm::vec4 { 1.0f };
            }
        }
    }

    static void loadIndices(std::vector<std::uint32_t>& indices, const tinygltf::Model& model, const tinygltf::Primitive& primitive) {
        ZoneScoped;
        const tinygltf::Accessor& accessor = model.accessors[primitive.indices];

        // TODO: support compressed meshes
        indices.resize(accessor.count);

        // TODO: parallelization?
        for (int j = 0; j < indices.size(); ++j) {
            switch(accessor.componentType) {
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: // u8
                    indices[j] = readFromAccessor<std::uint8_t>(j, accessor, model);
                    break;

                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: // u16
                    indices[j] = readFromAccessor<std::uint16_t>(j, accessor, model);
                    break;

                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: // u32
                    indices[j] = readFromAccessor<std::uint32_t>(j, accessor, model);
                    break;

                default:
                    verify(false, Carrot::sprintf("Unsupported component type: %d", accessor.componentType));
                    break;
            }
        }
    }

    LoadedScene GLTFLoader::load(const IO::Resource& resource) {
        ZoneScoped;

        tinygltf::TinyGLTF parser;
        tinygltf::FsCallbacks callbacks;

        callbacks.ReadWholeFile = gltfReadWholeFile;
        callbacks.ExpandFilePath = gltfExpandFilePath;
        callbacks.FileExists = gltfFileExists;
        callbacks.WriteWholeFile = nullptr;

        callbacks.user_data = (void*)&resource;
        parser.SetFsCallbacks(callbacks);

        tinygltf::Model model;
        std::string errors;
        std::string warnings;

        IO::VFS::Path vfsPath { resource.getName() };

        if(!parser.LoadASCIIFromFile(&model, &errors, &warnings, resource.getName())) {
            Carrot::Log::error("Failed to load glTF '%s': %s", resource.getName().c_str(), errors.c_str());
            TODO; // throw exception
        }

        LoadedScene result = load(model, vfsPath);
        result.debugName = resource.getName();

        return std::move(result);
    }

    LoadedScene GLTFLoader::load(const tinygltf::Model& model, const IO::VFS::Path& modelFilepath) {
        LoadedScene result;
        for(const auto& requiredExtension : model.extensionsRequired) {
            bool supported = false;
            for(const auto& supportedExtension : SUPPORTED_EXTENSIONS) {
                if(requiredExtension == supportedExtension) {
                    supported = true;
                    break;
                }
            }

            if(!supported) {
                throw std::runtime_error("Unsupported extension: " + requiredExtension);
            }
        }

        // 1. Load materials
        // 2. Load each scene
        // 3. For each scene, load node hierarchy and load meshes as they arrive
        // 4. TODO: Load animations, but I'm too lazy to do it now

        IO::VFS::Path nullPath{};
        std::vector<IO::VFS::Path> imagePaths;
        imagePaths.reserve(model.images.size());
        for(const auto& image : model.images) {

            imagePaths.emplace_back(modelFilepath.relative(IO::Path(decodeURI(image.uri))));
        }

        result.materials.reserve(model.materials.size());
        for(const auto& material : model.materials) {
            auto getTexturePath = [&](int textureIndex) -> const IO::VFS::Path& {
                if(textureIndex < 0)
                    return nullPath;

                const tinygltf::Texture& texture = model.textures[textureIndex];
                auto it = texture.extensions.find(KHR_TEXTURE_BASISU_EXTENSION_NAME);

                int source = -1;
                if(it != texture.extensions.end()) {
                    source = it->second.Get<tinygltf::Value::Object>().at("source").Get<int>();
                } else {
                    source = texture.source;
                }
                return imagePaths[source];
            };

            auto& loadedMaterial = result.materials.emplace_back();
            loadedMaterial.name = material.name;
            loadedMaterial.metallicFactor = static_cast<float>(material.pbrMetallicRoughness.metallicFactor);
            loadedMaterial.roughnessFactor = static_cast<float>(material.pbrMetallicRoughness.roughnessFactor);
            loadedMaterial.baseColorFactor = toVec4(material.pbrMetallicRoughness.baseColorFactor, glm::vec4{1.0f});
            loadedMaterial.emissiveFactor = toVec3(material.emissiveFactor, glm::vec3{1.0f});

            loadedMaterial.albedo = getTexturePath(material.pbrMetallicRoughness.baseColorTexture.index);
            loadedMaterial.normalMap = getTexturePath(material.normalTexture.index);
            loadedMaterial.occlusion = getTexturePath(material.occlusionTexture.index);
            loadedMaterial.emissive = getTexturePath(material.emissiveTexture.index);
            loadedMaterial.metallicRoughness = getTexturePath(material.pbrMetallicRoughness.metallicRoughnessTexture.index);

            if(material.alphaMode.empty()) {
                loadedMaterial.blendMode = LoadedMaterial::BlendMode::None;
            } else if(material.alphaMode == "BLEND") {
                loadedMaterial.blendMode = LoadedMaterial::BlendMode::Blend;
            }
        }

        std::vector<GLTFMesh> meshes;
        meshes.reserve(model.meshes.size());
        for(const auto& mesh : model.meshes) {
            GLTFMesh& gltfMesh = meshes.emplace_back();
            gltfMesh.firstPrimitive = result.primitives.size();

            for(const auto& primitive : mesh.primitives) {
                LoadedPrimitive& loadedPrimitive = result.primitives.emplace_back();
                loadedPrimitive.materialIndex = primitive.material;
                loadedPrimitive.name = mesh.name;

                PrimitiveInformation info;
                loadVertices(loadedPrimitive, model, primitive, info);
                loadIndices(loadedPrimitive.indices, model, primitive);

                loadedPrimitive.hadNormals = info.hasNormals;
                loadedPrimitive.hadTexCoords = info.hasTexCoords;
                loadedPrimitive.hadTangents = info.hasTangents;

                gltfMesh.primitiveCount++;
            }
        }

        result.nodeHierarchy = std::make_unique<Carrot::Render::Skeleton>(glm::mat4{1.0f});
        for(const auto& scene : model.scenes) {
            auto& sceneRoot = result.nodeHierarchy->hierarchy.newChild();
            for(const auto& nodeIndex : scene.nodes) {
                loadNodesRecursively(result, model, nodeIndex, meshes, sceneRoot, glm::mat4{1.0f});
            }
        }

        return std::move(result);
    }

    void GLTFLoader::loadNodesRecursively(LoadedScene& scene, const tinygltf::Model& model, int nodeIndex, const std::span<const GLTFMesh>& meshes, SkeletonTreeNode& parentNode, glm::mat4 parentTransform) {
        ZoneScoped;

        SkeletonTreeNode& newNode = parentNode.newChild();
        const tinygltf::Node& node = model.nodes[nodeIndex];
        glm::mat4 localTransform{1.0f};
        if(!node.matrix.empty()) {
            // transform given as column-major matrix
            for (int x = 0; x < 4; ++x) {
                for (int y = 0; y < 4; ++y) {
                    localTransform[x][y] = node.matrix[y + x * 4];
                }
            }

        } else {
            // transform given as translation, rotation and scale
            localTransform =
                    glm::translate(glm::mat4{1.0f}, toVec3(node.translation, glm::vec3{0.0f})) *
                    glm::toMat4(toQuat(node.rotation)) *
                    glm::scale(glm::mat4{1.0f}, toVec3(node.scale, glm::vec3{1.0f}));
        }

        const glm::mat4 transform = localTransform;
        newNode.bone.name = node.name;
        newNode.bone.originalTransform = transform;
        newNode.bone.transform = transform;

        if(node.mesh != -1) {
            newNode.meshIndices = std::vector<std::size_t>{};
            const GLTFMesh& mesh = meshes[node.mesh];
            for (std::size_t i = 0; i < mesh.primitiveCount; ++i) {
                newNode.meshIndices.value().push_back(i + mesh.firstPrimitive);
            }
        }

        for(const auto& childIndex : node.children) {
            loadNodesRecursively(scene, model, childIndex, meshes, newNode, transform);
        }
    }
}
