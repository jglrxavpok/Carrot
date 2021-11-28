//
// Created by jglrxavpok on 27/11/2021.
//

#pragma once

#include <filesystem>
#include <string>
#include <core/io/Resource.h>
#include <core/utils/JSON.h>

namespace ShaderCompiler {
    struct Metadata {
        std::vector<std::filesystem::path> sourceFiles; // all source files used to create this shader (original .glsl + included files)
        std::string command; // command to launch to recompile this shader

        // Initializes an empty metadata struct
        explicit Metadata() = default;

        // Initializes from JSON data
        explicit Metadata(const rapidjson::Value& json);

        // Serializes this struct to JSON. Same format as taken as input by the constructor
        void writeJSON(rapidjson::Document& document) const;

    };
}
