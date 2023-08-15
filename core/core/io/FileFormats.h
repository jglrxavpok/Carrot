//
// Created by jglrxavpok on 18/06/2021.
//

#pragma once

#include <filesystem>
#include <cstdint>
#include "core/utils/stringmanip.h"

namespace Carrot::IO {
    enum class FileFormat: std::uint8_t {
        UNKNOWN = 0,

        // stb_image
        PNG,
        JPEG,
        TGA,
        BMP,
        PSD,
        GIF,
        HDR,
        PIC,
        PNM,

        EXR,
        KTX2,

        ImageFirst = PNG,
        ImageLast = KTX2,

        FBX,
        OBJ,
        GLTF,
        ModelFirst = FBX,
        ModelLast = GLTF,

        LUA,
        ScriptFirst = LUA,
        ScriptLast = LUA,

        JSON,
        TXT,

        CNAV, // Carrot navmeshes
    };

    constexpr bool isImageFormat(FileFormat format) {
        return format >= FileFormat::ImageFirst && format <= FileFormat::ImageLast;
    }

    constexpr bool isModelFormat(FileFormat format) {
        return format >= FileFormat::ModelFirst && format <= FileFormat::ModelLast;
    }

    constexpr bool isScriptFormat(FileFormat format) {
        return format >= FileFormat::ScriptFirst && format <= FileFormat::ScriptLast;
    }

    inline FileFormat getFileFormat(const char* filepath) {
        std::filesystem::path p = filepath;
        std::u8string extensionU8 = p.extension().u8string();
        std::string extension = Carrot::toString(extensionU8);
#define CHECK(ext) \
    do {           \
        if(_stricmp(extension.c_str(), "." #ext) == 0) { \
            return FileFormat:: ext; \
        } \
    } while(0)
        CHECK(PNG);
        CHECK(JPEG);
        if(_stricmp(extension.c_str(), ".jpg") == 0) {
            return FileFormat::JPEG;
        }

        CHECK(TGA);
        CHECK(BMP);
        CHECK(PSD);
        CHECK(GIF);
        CHECK(HDR);
        CHECK(PIC);
        CHECK(PNM);

        CHECK(EXR);
        CHECK(KTX2);

        CHECK(FBX);
        CHECK(OBJ);
        CHECK(GLTF);

        CHECK(LUA);

        CHECK(JSON);
        CHECK(TXT);

        CHECK(CNAV);
        return FileFormat::UNKNOWN;
    }

    inline bool isImageFormat(const char* filepath) {
        return isImageFormat(getFileFormat(filepath));
    }

    inline bool isImageFormatFromPath(const std::filesystem::path& path) {
        std::string extension = Carrot::toString(path.u8string());
        return isImageFormat(extension.c_str());
    }

    inline bool isModelFormat(const char* filepath) {
        return isModelFormat(getFileFormat(filepath));
    }

    inline bool isModelFormatFromPath(const std::filesystem::path& path) {
        std::string extension = Carrot::toString(path.u8string());
        return isModelFormat(extension.c_str());
    }

    inline bool isScriptFormat(const char* filepath) {
        return isScriptFormat(getFileFormat(filepath));
    }

    inline bool isScriptFormatFromPath(const std::filesystem::path& path) {
        std::string extension = Carrot::toString(path.u8string());
        return isScriptFormat(extension.c_str());
    }
}