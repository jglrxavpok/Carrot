//
// Created by jglrxavpok on 04/11/2022.
//

#include <Fertilizer.h>
#include <TextureCompression.h>
#include <EnvironmentMapProcessing.h>
#include <models/ModelProcessing.h>
#include <iostream>
#include <unordered_map>
#include <filesystem>
#include "core/utils/stringmanip.h"

namespace Fertilizer {
    using fspath = std::filesystem::path;

    struct Convertor {
        std::filesystem::path replacementExtension;
        ConversionFunction func;
    };

    static std::unordered_map<std::string, Convertor> ConversionFunctions = {
            { ".png", { ".ktx2", compressTexture } },
            { ".jpg", { ".ktx2",  compressTexture } },
            { ".jpeg", { ".ktx2", compressTexture } },
            { ".tga", { ".ktx2",  compressTexture } },
            { ".bmp", { ".ktx2",  compressTexture } },
            { ".psd", { ".ktx2",  compressTexture } },
            { ".gif", { ".ktx2",  compressTexture } },
            { ".pic", { ".ktx2",  compressTexture } },
            { ".pnm", { ".ktx2",  compressTexture } },

            { ".gltf", { ".gltf", processGLTF } },
            { ".glb", { ".gltf", processGLTF } },
            { ".obj", { ".gltf", processAssimp } },
            { ".fbx", { ".gltf", processAssimp } },

        { ".hdr", { ".ktx2",  processEnvironmentMap } },
    };

    bool isSupportedFormat(const fspath& input) {
        return ConversionFunctions.find(input.extension().string()) != ConversionFunctions.end();
    }

    bool requiresModifications(const fspath& inputFile, const fspath& outputFile) {
        if(!std::filesystem::exists(outputFile))
            return true;
        return std::filesystem::last_write_time(inputFile) != std::filesystem::last_write_time(outputFile);
    }

    std::filesystem::path makeOutputPath(const std::filesystem::path& inputFile) {
        auto convertorIt = ConversionFunctions.find(inputFile.extension().string());
        if(convertorIt == ConversionFunctions.end()) {
            return {};
        }

        auto newPath = inputFile;
        return newPath.replace_extension(convertorIt->second.replacementExtension);
    }

    static void makeTimestampsMatch(const fspath& input, const fspath& output) {
        auto inputTimestamp = std::filesystem::last_write_time(input);
        std::filesystem::last_write_time(output, inputTimestamp);
    }

    ConversionResult copyConvert(const fspath& inputFile, const fspath& outputFile) {
        fspath outputFolder = outputFile.parent_path();
        if(!std::filesystem::exists(outputFolder)) {
            std::filesystem::create_directories(outputFolder);
        }
        std::filesystem::copy(inputFile, outputFile, std::filesystem::copy_options::overwrite_existing);
        makeTimestampsMatch(inputFile, outputFile);
        return {};
    }

    ConversionResult convert(const fspath& inputFile, const fspath& outputFile, bool forceConvert) {
        auto convertorIt = ConversionFunctions.find(inputFile.extension().string());
        if(convertorIt == ConversionFunctions.end()) {
            return {
                .errorCode = ConversionResultError::UnsupportedInputType,
                .errorMessage = Carrot::sprintf("Unsupported extension: %s", inputFile.extension().string().c_str())
            };
        }

        if(convertorIt->second.replacementExtension != outputFile.extension()) {
            return {
                .errorCode = ConversionResultError::UnsupportedOutputType,
                .errorMessage = Carrot::sprintf("Expected output extension '%s', got '%s'", convertorIt->second.replacementExtension.string().c_str(), outputFile.extension().c_str()),
            };
        }

        if(!forceConvert && !requiresModifications(inputFile, outputFile)) {
            return {
                .errorCode = ConversionResultError::Success,
                .errorMessage = "Asset already up-to-date",
            };
        }

        fspath outputFolder = outputFile.parent_path();
        if(!std::filesystem::exists(outputFolder)) {
            std::filesystem::create_directories(outputFolder);
        }

        ConversionResult result = convertorIt->second.func(inputFile, outputFile);

        if(result.errorCode == ConversionResultError::Success) {
            makeTimestampsMatch(inputFile, outputFile);
        }

        return result;
    }
}