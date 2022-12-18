//
// Created by jglrxavpok on 05/11/2022.
//

#include <GLTFProcessing.h>
#include <core/utils/CarrotTinyGLTF.h>
#include <core/scene/LoadedScene.h>
#include <core/Macros.h>
#include <core/scene/GLTFLoader.h>
#include <GLTFWriter.h>
#include <core/io/vfs/VirtualFileSystem.h>
#include <unordered_set>

namespace Fertilizer {

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

    struct ExpandedVertex {
        std::uint32_t originalIndex = 0;

        std::optional<std::uint32_t> newIndex;
        Carrot::Vertex vertex{};
    };

    struct ExpandedMesh {
        std::vector<ExpandedVertex> vertices;

        std::vector<std::vector<std::uint32_t>> duplicatedVertices; // per vertex in the original vertex buffer, where are the copies inside the expanded 'vertices' array
    };

    /**
     * "Expands" the vertex buffer: this is the exact opposite of indexing, we separate vertex info for each face
     *  otherwise keeping the same index buffer will provide incorrect results after attribute generation
     */
    static ExpandedMesh expandMesh(const LoadedPrimitive& primitive) {
        ExpandedMesh expanded;
        // TODO: handle skinning
        const std::size_t vertexCount = primitive.vertices.size();
        const std::size_t indexCount = primitive.indices.size();
        expanded.vertices.resize(indexCount);
        expanded.duplicatedVertices.resize(vertexCount);
        for(std::size_t i = 0; i < indexCount; i++) {
            const std::uint32_t index = primitive.indices[i];
            expanded.vertices[i].vertex = primitive.vertices[index];
            expanded.vertices[i].originalIndex = index;

            expanded.duplicatedVertices[index].push_back(i);
        }

        return std::move(expanded);
    }

    static bool areSameVertices(const Carrot::Vertex& a, const Carrot::Vertex& b) {
        return memcmp(&a, &b, sizeof(Carrot::Vertex)) == 0; // TODO: be less strict, this only works because we don't modify the vertices for now
    }

    /**
     * Generate indexed mesh into 'out' from a non-indexed mesh inside ExpandedMesh
     */
    static void collapseMesh(LoadedPrimitive& out, ExpandedMesh& mesh) {
        out.vertices.clear();
        out.indices.clear();
        std::uint32_t nextIndex = 0;
        for(std::size_t vertexIndex = 0; vertexIndex < mesh.vertices.size(); vertexIndex++) {
            auto& duplicatedVertex = mesh.vertices[vertexIndex];
            verify(!duplicatedVertex.newIndex.has_value(), "Programming error: duplicated vertex must not already have an index in the new mesh");
            const std::vector<std::uint32_t>& siblingIndices = mesh.duplicatedVertices[duplicatedVertex.originalIndex];

            // check if any sibling is still the same as our current vertex, and has already been written to vertex buffer
            std::optional<std::uint32_t> indexToReuse;
            for(std::uint32_t siblingIndex : siblingIndices) {
                const ExpandedVertex& sibling = mesh.vertices[siblingIndex];
                if(!sibling.newIndex.has_value()) {
                    continue; // not already in vertex buffer
                }

                if(areSameVertices(sibling.vertex, duplicatedVertex.vertex)) {
                    indexToReuse = sibling.newIndex.value();
                    break;
                }
            }

            if(indexToReuse.has_value()) {
                out.indices.push_back(indexToReuse.value());
            } else {
                // no index to reuse, create new index and write to vertex buffer

                duplicatedVertex.newIndex = nextIndex;
                out.indices.push_back(nextIndex);
                nextIndex++;
                out.vertices.emplace_back(duplicatedVertex.vertex);
            }
        }
    }

    void generateMissingAttributes(tinygltf::Model& model) {
        GLTFLoader loader{};
        LoadedScene scene = loader.load(model, {});

        for(auto& primitive : scene.primitives) {
            // no need to regenerate anything if every attribute is already present
            if(primitive.hadTexCoords && primitive.hadNormals && primitive.hadTangents) {
                continue;
            }

            ExpandedMesh expandedMesh = expandMesh(primitive);
            // TODO: generate missing attributes
            collapseMesh(primitive, expandedMesh);
        }

        // re-export model
        tinygltf::Model reexported = std::move(writeAsGLTF(scene));
        // keep copyright+author info
        model.asset.extras = std::move(model.asset.extras);
        model.asset.copyright = std::move(model.asset.copyright);

        model = std::move(reexported);
    }

    void convertTexturePaths(tinygltf::Model& model, fspath inputFile, fspath outputFile) {
        fspath parentPath = inputFile.parent_path();
        fspath outputParentPath = outputFile.parent_path();

        const std::vector<std::string> extensionsRequired = {
                "KHR_texture_basisu",
        };

        for(const auto& ext : extensionsRequired) {
            model.extensionsRequired.push_back(ext);
            model.extensionsUsed.push_back(ext);
        }

        std::unordered_set<std::size_t> modifiedImages;

        modifiedImages.reserve(model.images.size());
        for(std::size_t imageIndex = 0; imageIndex < model.images.size(); imageIndex++) {
            tinygltf::Image& image = model.images[imageIndex];
            fspath uri = image.uri;
            uri = Fertilizer::makeOutputPath(uri);
            image.uri = uri.string();

            modifiedImages.insert(imageIndex);
        }

        for(std::size_t textureIndex = 0; textureIndex < model.textures.size(); textureIndex++) {
            tinygltf::Texture& texture = model.textures[textureIndex];
            if(modifiedImages.contains(texture.source)) {
                tinygltf::Value::Object extensionContents;
                extensionContents["source"] = tinygltf::Value{ texture.source };
                texture.extensions[KHR_TEXTURE_BASISU_EXTENSION_NAME] = tinygltf::Value{ extensionContents };
                texture.source = -1;
            }
        }
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

        // buffers are regenerated inside 'generateMissingAttributes' method too, so we don't copy the .bin file
        generateMissingAttributes(model);

        convertTexturePaths(model, inputFile, outputFile);

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