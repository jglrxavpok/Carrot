//
// Created by jglrxavpok on 27/10/2022.
//

#include "GLTFLoader.h"
#include "core/io/Logging.hpp"
#include <core/utils/Profiling.h>
#include <glm/gtx/quaternion.hpp>
#include <set>

#include "core/io/vfs/VirtualFileSystem.h"
#include "core/tasks/Tasks.h"

namespace Carrot::Render {

    static glm::mat4 glTFSpaceToCarrotSpace = glm::rotate(glm::mat4{1.0f}, glm::pi<float>()/2.0f, glm::vec3(1,0,0));

    constexpr const char* const KHR_TEXTURE_BASISU_EXTENSION_NAME = "KHR_texture_basisu";

    constexpr const char* const SUPPORTED_EXTENSIONS[] = {
            KHR_TEXTURE_BASISU_EXTENSION_NAME,
            GLTFLoader::CARROT_MESHLETS_EXTENSION_NAME
    };

    struct PrimitiveInformation {
        bool hasTangents = false;
        bool hasTexCoords = false;
        bool hasNormals = false;
    };

    static std::string getNodeName(const tinygltf::Model& model, int nodeIndex) {
        auto& node = model.nodes[nodeIndex];
        if(node.name.empty()) {
            return Carrot::sprintf("Unnamed node %d", nodeIndex);
        }
        return node.name;
    }

    bool gltfReadWholeFile(std::vector<unsigned char>* out,
                           std::string* err, const std::string& filepath,
                           void* userData) {
        ZoneScoped;
        IO::FileHandle file { filepath, IO::OpenMode::Read };
        std::uint64_t resourceSize = file.getSize();
        out->resize(resourceSize);
        file.read(out->data(), resourceSize);
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
/*        const IO::Resource& modelResource = *(static_cast<const IO::Resource*>(userData));
        IO::VFS::Path p { abs_filename };
        return GetVFS().exists(p);*/
        return std::filesystem::exists(abs_filename);
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
        std::vector<SkinnedVertex>& skinnedVertices = loadedPrimitive.skinnedVertices;
        const tinygltf::Accessor& positionsAccessor = model.accessors[primitive.attributes.at("POSITION")];

        int normalsAccessorIndex = -1;
        auto normalsAttributeIt = primitive.attributes.find("NORMAL");
        if(normalsAttributeIt != primitive.attributes.end()) {
            normalsAccessorIndex = normalsAttributeIt->second;
            info.hasNormals = true;
        }

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

        int jointsAccessorIndex = -1;
        auto jointsAttributeIt = primitive.attributes.find("JOINTS_0");
        if(jointsAttributeIt != primitive.attributes.end()) {
            jointsAccessorIndex = jointsAttributeIt->second;
        }

        int jointWeightsAccessorIndex = -1;
        auto jointWeightsAttributeIt = primitive.attributes.find("WEIGHTS_0");
        if(jointsAttributeIt != primitive.attributes.end()) {
            jointWeightsAccessorIndex = jointWeightsAttributeIt->second;
        }

        const tinygltf::Accessor* normalsAccessor = normalsAccessorIndex != -1 ? &model.accessors[normalsAccessorIndex] : nullptr;
        const tinygltf::Accessor* vertexColorAccessor = colorAccessorIndex != -1 ? &model.accessors[colorAccessorIndex] : nullptr;
        const tinygltf::Accessor* texCoordsAccessor = texCoordsAccessorIndex != -1 ? &model.accessors[texCoordsAccessorIndex] : nullptr;
        const tinygltf::Accessor* tangentsAccessor = tangentAccessorIndex != -1 ? &model.accessors[tangentAccessorIndex] : nullptr;
        const tinygltf::Accessor* jointsAccessor = jointsAccessorIndex != -1 ? &model.accessors[jointsAccessorIndex] : nullptr;
        const tinygltf::Accessor* jointWeightsAccessor = jointWeightsAccessorIndex != -1 ? &model.accessors[jointWeightsAccessorIndex] : nullptr;

        const bool usesSkinning = jointsAccessor != nullptr && jointWeightsAccessor != nullptr;
        loadedPrimitive.isSkinned = usesSkinning;
        // TODO: UV2
        // TODO: support compressed meshes

        if(normalsAccessor != nullptr) {
            verify(positionsAccessor.count == normalsAccessor->count, "Mismatched position/normal count");
        }

        if(texCoordsAccessor != nullptr) {
            verify(positionsAccessor.count == texCoordsAccessor->count, "Mismatched position/texCoords count");
        }

        if(tangentsAccessor != nullptr) {
            verify(positionsAccessor.count == tangentsAccessor->count, "Mismatched position/tangents count");
        }

        if(jointsAccessor != nullptr) {
            verify(positionsAccessor.count == jointsAccessor->count, "Mismatched position/joints count");
        }

        if(jointWeightsAccessor != nullptr) {
            verify(positionsAccessor.count == jointWeightsAccessor->count, "Mismatched position/jointWeights count");
        }

        loadedPrimitive.minPos = toVec3(positionsAccessor.minValues, glm::vec3{INFINITY});
        loadedPrimitive.maxPos = toVec3(positionsAccessor.maxValues, glm::vec3{-INFINITY});

        if(usesSkinning) {
            skinnedVertices.resize(positionsAccessor.count);
        } else {
            vertices.resize(positionsAccessor.count);
        }
        for (std::size_t i = 0; i < positionsAccessor.count; ++i) {
            Carrot::Vertex& vertex = usesSkinning ? skinnedVertices[i] : vertices[i];
            const glm::vec3 pos = readFromAccessor<glm::vec3>(i, positionsAccessor, model);
            vertex.pos = glm::vec4 { pos, 1.0f };
            loadedPrimitive.minPos = glm::min(loadedPrimitive.minPos, pos);
            loadedPrimitive.maxPos = glm::max(loadedPrimitive.maxPos, pos);

            if(normalsAccessor) {
                vertex.normal = readFromAccessor<glm::vec3>(i, *normalsAccessor, model);
            } else {
                vertex.normal = glm::vec3(0.0f, 0.0f, 1.0f);
            }

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

            if(usesSkinning) {
                SkinnedVertex& skinnedVertex = (SkinnedVertex&)vertex;
                skinnedVertex.boneIDs = readFromAccessor<glm::u8vec4>(i, *jointsAccessor, model);
                skinnedVertex.boneWeights = readFromAccessor<glm::vec4>(i, *jointWeightsAccessor, model);
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

    static void loadMeshlets(LoadedPrimitive& loadedPrimitive, const tinygltf::Model& model, const tinygltf::Primitive& primitive) {
        auto iter = primitive.extensions.find(GLTFLoader::CARROT_MESHLETS_EXTENSION_NAME);
        if(iter == primitive.extensions.end()) {
            return;
        }

        const tinygltf::Value& value = iter->second;
        const int meshletsAccessorIndex = value.Get("meshlets").GetNumberAsInt();
        const int meshletsVertexIndicesAccessorIndex = value.Get("meshlets_vertex_indices").GetNumberAsInt();
        const int meshletsIndicesAccessorIndex = value.Get("meshlets_indices").GetNumberAsInt();

        {
            const tinygltf::Accessor& accessor = model.accessors[meshletsAccessorIndex];
            loadedPrimitive.meshlets.resize(accessor.count / sizeof(Carrot::Render::Meshlet));
            for(std::size_t i = 0; i < loadedPrimitive.meshlets.size(); i++) {
                loadedPrimitive.meshlets[i] = readFromAccessor<Carrot::Render::Meshlet>(i, accessor, model);
            }
            //Carrot::Async::parallelFor(loadedPrimitive.meshlets.size(), [&](std::size_t i) {
            //  }, 16);
        }
        {
            const tinygltf::Accessor& accessor = model.accessors[meshletsVertexIndicesAccessorIndex];
            loadedPrimitive.meshletVertexIndices.resize(accessor.count);
            for(std::size_t i = 0; i < loadedPrimitive.meshletVertexIndices.size(); i++) {
                //Carrot::Async::parallelFor(loadedPrimitive.meshletVertexIndices.size(), [&](std::size_t i) {
                loadedPrimitive.meshletVertexIndices[i] = readFromAccessor<std::uint32_t>(i, accessor, model);
                //}, 16);
            }
        }
        {
            const tinygltf::Accessor& accessor = model.accessors[meshletsIndicesAccessorIndex];
            loadedPrimitive.meshletIndices.resize(accessor.count);
            for(std::size_t i = 0; i < loadedPrimitive.meshletIndices.size(); i++) {
            //Carrot::Async::parallelFor(loadedPrimitive.meshletIndices.size(), [&](std::size_t i) {
                loadedPrimitive.meshletIndices[i] = readFromAccessor<std::uint32_t>(i, accessor, model);
            //}, 16);
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

        std::string gltfContents = resource.readText();
        const char* gltfStr = gltfContents.c_str();
        const std::size_t gltfStrSize = gltfContents.size();

        std::string baseDir = Carrot::toString(resource.getFilepath().parent_path().u8string());

        if(!parser.LoadASCIIFromString(&model, &errors, &warnings, gltfStr, gltfStrSize, baseDir)) {
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
                //throw std::runtime_error("Unsupported extension: " + requiredExtension);
                Carrot::Log::error("Unsupported extension: " + requiredExtension);
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

        bool hasAnySkin = false;
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
                loadMeshlets(loadedPrimitive, model, primitive);
                hasAnySkin |= loadedPrimitive.isSkinned;

                loadedPrimitive.hadNormals = info.hasNormals;
                loadedPrimitive.hadTexCoords = info.hasTexCoords;
                loadedPrimitive.hadTangents = info.hasTangents;

                gltfMesh.primitiveCount++;
            }
        }

        NodeMapping nodeMapping;
        result.nodeHierarchy = std::make_unique<Carrot::Render::Skeleton>(glm::mat4{1.0f});
        result.nodeHierarchy->hierarchy.bone.transform = result.nodeHierarchy->hierarchy.bone.originalTransform = glTFSpaceToCarrotSpace;
        result.nodeHierarchy->hierarchy.bone.name = "Model root " + modelFilepath.toString();
        for(const auto& scene : model.scenes) {
            auto& sceneRoot = result.nodeHierarchy->hierarchy.newChild();
            sceneRoot.bone.name = "glTF Scene root " + result.debugName;
            for(const auto& nodeIndex : scene.nodes) {
                loadNodesRecursively(result, model, nodeIndex, meshes, sceneRoot, nodeMapping, glTFSpaceToCarrotSpace);
            }
        }

        if(hasAnySkin) {
           loadAnimations(result, model, nodeMapping);
        }

        return std::move(result);
    }

    void GLTFLoader::loadAnimations(LoadedScene& result, const tinygltf::Model& model, const NodeMapping& nodeMapping) {
        for(const auto& animation : model.animations) {
            const std::size_t animationIndex = result.animationData.size();
            Animation& carrotAnimation = result.animationData.emplace_back();

            std::string animationName = animation.name;
            if(animationName.empty()) {
                animationName = Carrot::sprintf("animation %d", animationIndex);
            }
            result.animationMapping[animationName] = animationIndex;

            // mimic what is done in AssimpLoader: load all timestamps and fill translation/rotation/scale of each bone for each timestamp
            //  not great for memory, but dumb enough for fast runtime usage

            std::set<float> timestampsSet; // we want them sorted
            float duration = 0.0f;
            for(const auto& channel : animation.channels) {
                const auto& sampler = animation.samplers[channel.sampler];
                const auto& timestampAccessor = model.accessors[sampler.input];
                for(std::size_t timestampIndex = 0; timestampIndex < timestampAccessor.count; timestampIndex++) {
                    const float timestamp = readFromAccessor<float>(timestampIndex, timestampAccessor, model);
                    timestampsSet.emplace(timestamp);
                    carrotAnimation.duration = std::max(timestamp, carrotAnimation.duration);
                }
            }

            std::vector<float> allTimestamps;
            allTimestamps.reserve(timestampsSet.size());
            for(const float& timestamp : timestampsSet) {
                allTimestamps.emplace_back(timestamp);
            }

            carrotAnimation.keyframeCount = allTimestamps.size();
            carrotAnimation.keyframes.resize(carrotAnimation.keyframeCount);
            for(auto& keyframe : carrotAnimation.keyframes) {
                keyframe.boneTransforms.resize(nodeMapping.size(), glm::mat4{1.0f});
            }

            // read the TRS of each node over time for this animation
            struct GLTFKeyframe {
                std::optional<glm::vec3> position;
                std::optional<glm::quat> rotation;
                std::optional<glm::vec3> scale;

                bool worldSpace = false;
                glm::mat4 worldSpaceTransform = glm::identity<glm::mat4>();
            };

            std::unordered_map<std::uint32_t, std::vector<GLTFKeyframe>> keyframesForAllNodes;
            std::size_t keyframeIndex = 0;
            for(const float& timestamp : allTimestamps) {
                Keyframe& carrotKeyframe = carrotAnimation.keyframes[keyframeIndex];
                carrotKeyframe.timestamp = timestamp;
                for(const auto& channel : animation.channels) {
                    const auto& sampler = animation.samplers[channel.sampler];
                    const auto& timestampAccessor = model.accessors[sampler.input];
                    const auto& keyframeValueAccessor = model.accessors[sampler.output];

                    std::vector<GLTFKeyframe>& keyframes = keyframesForAllNodes[channel.target_node];
                    if(keyframes.empty()) {
                        keyframes.resize(allTimestamps.size());
                    }

                    // find which keyframe corresponds to the given timestamp
                    std::size_t timestampIndex = 0;
                    for(; timestampIndex < timestampAccessor.count; timestampIndex++) {
                        const float keyframeTimestamp = readFromAccessor<float>(timestampIndex, timestampAccessor, model);
                        if(keyframeTimestamp >= timestamp) {
                            break;
                        }
                    }

                    GLTFKeyframe& keyframe = keyframes[timestampIndex];

                    const std::string& target = channel.target_path;
                    if(target == "translation") {
                        keyframe.position = readFromAccessor<glm::vec3>(timestampIndex, keyframeValueAccessor, model);
                    } else if(target == "rotation") {
                        glm::vec4 keyframeRotationXYZW = readFromAccessor<glm::vec4>(timestampIndex, keyframeValueAccessor, model);
                        keyframe.rotation = { keyframeRotationXYZW.w, keyframeRotationXYZW.x, keyframeRotationXYZW.y, keyframeRotationXYZW.z };
                    } else if(target == "scale") {
                        keyframe.scale = readFromAccessor<glm::vec3>(timestampIndex, keyframeValueAccessor, model);
                    } else {
                        verify(false, "Unknown target_path in glTF: " + target);
                    }
                }

                keyframeIndex++;
            }

            // interpolate keyframe values when none exist
            for(auto& [nodeID, keyframes] : keyframesForAllNodes) {
                // used if there are no more keyframes with a value at this timestamp (keep same keyframe value until end of animation)
                GLTFKeyframe latestKeyframe{
                        .position = glm::vec3{0.0f},
                        .rotation = glm::identity<glm::quat>(),
                        .scale = glm::vec3{1.0f}
                };
                auto interpolate = [&](auto pMemberPtr, std::size_t index) {
                    if (index == 0) {
                        return (latestKeyframe.*pMemberPtr).value();
                    }

                    // find next keyframe with a value
                    std::size_t nextIndex = index;
                    for (std::size_t i = index + 1; i < allTimestamps.size(); i++) {
                        if ((keyframes[i].*pMemberPtr).has_value()) {
                            nextIndex = i;
                            break;
                        }
                    }

                    if (nextIndex <= index) { // there is no keyframe after this one which contains a value
                        return (latestKeyframe.*pMemberPtr).value();
                    } else {
                        std::size_t previousIndex = index - 1;
                        const GLTFKeyframe& previousKeyframe = keyframes[previousIndex];
                        const GLTFKeyframe& nextKeyframe = keyframes[nextIndex];

                        float previousTime = allTimestamps[previousIndex];
                        float nextTime = allTimestamps[nextIndex];

                        float currentTime = allTimestamps[index];
                        float t = (currentTime - previousTime) / (nextTime - previousTime);
                        return (previousKeyframe.*pMemberPtr).value() * (1 - t) +
                               (nextKeyframe.*pMemberPtr).value() * t;
                    }
                };
                for (std::size_t i = 0; i < allTimestamps.size(); i++) {
                    GLTFKeyframe& currentKeyframe = keyframes[i];
                    if (!currentKeyframe.position.has_value()) {
                        currentKeyframe.position = interpolate(&GLTFKeyframe::position, i);
                    }
                    if (!currentKeyframe.rotation.has_value()) {
                        currentKeyframe.rotation = interpolate(&GLTFKeyframe::rotation, i);
                    }
                    if (!currentKeyframe.scale.has_value()) {
                        currentKeyframe.scale = interpolate(&GLTFKeyframe::scale, i);
                    }
                    latestKeyframe = currentKeyframe;
                }
            }

            for(auto& [nodeID, keyframes] : keyframesForAllNodes) {
                const int meshIndex = 0; // TODO: like AssimpLoader, only a single mesh can be animated at once per glTF file when loaded into Carrot
                // at this point, all keyframes have values
                // now compute global transform for each keyframe
                for(std::size_t timestampIndex = 0; timestampIndex < allTimestamps.size(); timestampIndex++) {
                    Keyframe& finalKeyframe = carrotAnimation.keyframes[timestampIndex];

                    std::function<glm::mat4(SkeletonTreeNode&, GLTFKeyframe&)> computeTransformRecursively =
                            [&](SkeletonTreeNode& treeNode, GLTFKeyframe& keyframe) -> glm::mat4 {
                                if(keyframe.worldSpace) {
                                    return keyframe.worldSpaceTransform;
                                }

                                glm::mat4 translationMat = glm::translate(glm::mat4{1.0f}, keyframe.position.value());
                                glm::mat4 rotationMat = glm::toMat4(glm::normalize(keyframe.rotation.value()));
                                glm::mat4 scalingMat = glm::scale(glm::mat4{1.0f}, keyframe.scale.value());
                                glm::mat4 localTransform = translationMat * rotationMat * scalingMat;

                                glm::mat4 parentMatrix = glm::identity<glm::mat4>();
                                if(treeNode.pParent) {
                                    auto parentIter = nodeMapping.find(treeNode.pParent);
                                    if(parentIter != nodeMapping.end()) { // == end if the root is the scene root, which is not a node inside glTF
                                        int parentNodeIndex = parentIter->second;
                                        auto iter = keyframesForAllNodes.find(parentNodeIndex);
                                        if(iter != keyframesForAllNodes.end()) {
                                            parentMatrix = computeTransformRecursively(*treeNode.pParent, iter->second[timestampIndex]);
                                        }
                                    }
                                }

                                glm::mat4 globalTransform = parentMatrix * localTransform;
                                keyframe.worldSpaceTransform = globalTransform;
                                keyframe.worldSpace = true;
                                return keyframe.worldSpaceTransform;
                            };

                    GLTFKeyframe& currentKeyframe = keyframes[timestampIndex];
                    const std::string& nodeName = getNodeName(model, nodeID);
                    auto boneMappingIter = result.boneMapping[meshIndex].find(nodeName);
                    if(boneMappingIter == result.boneMapping[meshIndex].end()) {
                        continue;
                    }
                    std::uint32_t boneIndex = boneMappingIter->second;
                    SkeletonTreeNode* pTreeNode = result.nodeHierarchy->findNode(nodeName);
                    verify(pTreeNode, "Could not find matching node in tree");
                    const glm::mat4& boneOffset = result.offsetMatrices[meshIndex].at(nodeName);
                    finalKeyframe.boneTransforms[boneIndex] = glTFSpaceToCarrotSpace * computeTransformRecursively(*pTreeNode, currentKeyframe) * boneOffset;
                }
            }
        }
    }

    void GLTFLoader::loadNodesRecursively(LoadedScene& scene, const tinygltf::Model& model, int nodeIndex, const std::span<const GLTFMesh>& meshes, SkeletonTreeNode& parentNode, NodeMapping& nodeMapping, const glm::mat4& parentTransform) {
        ZoneScoped;

        SkeletonTreeNode& newNode = parentNode.newChild();
        const tinygltf::Node& node = model.nodes[nodeIndex];
        glm::mat4 localTransform{1.0f};
        if(!node.matrix.empty()) {
            // transform given as column-major matrix
            for (int y = 0; y < 4; ++y) {
                for (int x = 0; x < 4; ++x) {
                    localTransform[x][y] = node.matrix[x + y * 4];
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
        newNode.bone.name = getNodeName(model, nodeIndex);
        newNode.bone.originalTransform = transform;
        newNode.bone.transform = transform;
        nodeMapping[&newNode] = nodeIndex;

        if(node.mesh != -1) {
            newNode.meshIndices = std::vector<std::size_t>{};
            const GLTFMesh& mesh = meshes[node.mesh];

            std::unordered_map<std::string, std::uint32_t> skinBoneMapping;
            std::unordered_map<std::string, glm::mat4> skinInverseBinds;

            if(node.skin != -1) {
                auto& glTFSkin = model.skins[node.skin];
                for(std::size_t jointIndex = 0; jointIndex < glTFSkin.joints.size(); jointIndex++) {
                    int jointNodeID = glTFSkin.joints[jointIndex];
                    const auto& jointNode = model.nodes[jointNodeID];
                    const std::string jointName = getNodeName(model, jointNodeID);
                    skinBoneMapping[jointName] = jointIndex;

                    if(glTFSkin.inverseBindMatrices != -1) {
                        skinInverseBinds[jointName] = readFromAccessor<glm::mat4>(jointIndex, model.accessors[glTFSkin.inverseBindMatrices], model);
                    } else {
                        skinInverseBinds[jointName] = glm::identity<glm::mat4>();
                    }
                }
            }

            const std::size_t skinMeshIndex = 0; // TODO: like AssimpLoader, only a single mesh can be animated at once per glTF file when loaded into Carrot
            for (std::size_t i = 0; i < mesh.primitiveCount; ++i) {
                const std::size_t meshIndex = i + mesh.firstPrimitive;
                newNode.meshIndices.value().push_back(meshIndex);

                if(node.skin != -1) {
                    for(auto& [k, v] : skinBoneMapping) {
                        scene.boneMapping[skinMeshIndex][k] = v;
                    }
                    for(auto& [k, v] : skinInverseBinds) {
                        scene.offsetMatrices[skinMeshIndex][k] = v;
                    }
                }
            }
        }

        for(const auto& childIndex : node.children) {
            loadNodesRecursively(scene, model, childIndex, meshes, newNode, nodeMapping, transform);
        }
    }
}
