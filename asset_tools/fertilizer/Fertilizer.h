//
// Created by jglrxavpok on 04/11/2022.
//

#pragma once

#include <filesystem>

namespace Fertilizer {
    enum class ConversionResultError {
        Success = 0,
        InputFileDoesNotExist,
        UnsupportedInputType,
        UnsupportedOutputType,
        TextureCompressionError,
        GLTFCompressionError,
    };

    struct ConversionResult {
        ConversionResultError errorCode = ConversionResultError::Success;
        std::string errorMessage;
    };

    using ConversionFunction = ConversionResult(*)(const std::filesystem::path& inputFile, const std::filesystem::path& outputFile);

    /**
     * Returns true iif the format of the given file path is one Fertilizer cares about.
     */
    bool isSupportedFormat(const std::filesystem::path& input);

    /**
     * Checks timestamps of inputFile and outputFile to check if the conversion needs to be redone.
     * Performs an equality check, not a >= check: input assets could be rolled back via a version control system and we
     * would need to rerun the conversion on the old asset.
     */
    bool requiresModifications(const std::filesystem::path& inputFile, const std::filesystem::path& outputFile);

    /**
     * Creates a new path with the extension of the input replaced with the extension used for the output.
     * NO GUARANTEE TO be different than input!
     */
    std::filesystem::path makeOutputPath(const std::filesystem::path& file);

    /**
     * Performs a copy of the input file to the output file, and modifies the timestamp of the output file to match the input's
     */
    ConversionResult copyConvert(const std::filesystem::path& inputFile, const std::filesystem::path& outputFile);

    ConversionResult convert(const std::filesystem::path& inputFile, const std::filesystem::path& outputFile, bool forceConvert);
}