//
// Created by jglrxavpok on 05/11/2022.
//

#include <GLTFCompression.h>
#include <core/utils/CarrotTinyGLTF.h>
#include "core/Macros.h"
#include <unordered_set>

namespace Fertilizer {

    constexpr const char* const KHR_TEXTURE_BASISU_EXTENSION_NAME = "KHR_texture_basisu";

    using fspath = std::filesystem::path;

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

    ConversionResult compressGLTF(const std::filesystem::path& inputFile, const std::filesystem::path& outputFile) {
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
        // convert

        std::vector<std::string> extensionsRequired = {
                "KHR_texture_basisu",
        };

        for(const auto& ext : extensionsRequired) {
            model.extensionsRequired.push_back(ext);
            model.extensionsUsed.push_back(ext);
        }

        for(const auto& buffer : model.buffers) {
            copyConvert(parentPath / buffer.uri, outputParentPath / buffer.uri);
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

        if(!parser.WriteGltfSceneToFile(&model, outputFile.string(), false, false, false, false)) {
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