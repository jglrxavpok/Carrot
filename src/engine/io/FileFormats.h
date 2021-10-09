//
// Created by jglrxavpok on 18/06/2021.
//

#pragma once

#include <filesystem>

namespace Carrot::IO {
    enum class FileFormat {
        PNG,
        JPEG,
    };

    constexpr bool isImageFormat(FileFormat format) {
        switch (format) {
            case FileFormat::PNG:
            case FileFormat::JPEG:
                return true;

            default: return false;
        }
    }

    inline bool isImageFormat(const char* fileFormat) {
        if(_stricmp(fileFormat, ".png") == 0) {
            return true;
        }
        if(_stricmp(fileFormat, ".jpeg") == 0) {
            return true;
        }
        if(_stricmp(fileFormat, ".jpg") == 0) {
            return true;
        }
        return false;
    }

    inline bool isImageFormatFromPath(const std::filesystem::path& path) {
        std::string extension = path.extension().string();
        return isImageFormat(extension.c_str());
    }
}