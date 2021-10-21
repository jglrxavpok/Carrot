//
// Created by jglrxavpok on 18/06/2021.
//

#pragma once

#include <filesystem>
#include <cstdint>
#include "engine/utils/stringmanip.h"

namespace Carrot::IO {
    enum class FileFormat: std::uint8_t {
        UNKNOWN = 0,

        PNG,
        JPEG,
        ImageFirst = PNG,
        ImageLast = JPEG,

        FBX,
        OBJ,
        ModelFirst = FBX,
        ModelLast = OBJ,
    };

    constexpr bool isImageFormat(FileFormat format) {
        return format >= FileFormat::ImageFirst && format <= FileFormat::ImageLast;
    }

    constexpr bool isModelFormat(FileFormat format) {
        return format >= FileFormat::ModelFirst && format <= FileFormat::ModelLast;
    }

    inline FileFormat getFileFormat(const char* filepath) {
        if(_stricmp(filepath, ".png") == 0) {
            return FileFormat::PNG;
        }
        if(_stricmp(filepath, ".jpeg") == 0 || _stricmp(filepath, ".jpg") == 0) {
            return FileFormat::JPEG;
        }
        if(_stricmp(filepath, ".fbx") == 0) {
            return FileFormat::FBX;
        }
        if(_stricmp(filepath, ".obj") == 0) {
            return FileFormat::OBJ;
        }
        return FileFormat::UNKNOWN;
    }

    inline bool isImageFormat(const char* filepath) {
        return isImageFormat(getFileFormat(filepath));
    }

    inline bool isImageFormatFromPath(const std::filesystem::path& path) {
        std::string extension = Carrot::toString(path.extension().u8string());
        return isImageFormat(extension.c_str());
    }

    inline bool isModelFormat(const char* filepath) {
        return isModelFormat(getFileFormat(filepath));
    }

    inline bool isModelFormatFromPath(const std::filesystem::path& path) {
        std::string extension = Carrot::toString(path.extension().u8string());
        return isModelFormat(extension.c_str());
    }
}