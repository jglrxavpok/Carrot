//
// Created by jglrxavpok on 27/11/2021.
//

#include "ShaderMetadata.h"
#include <core/utils/stringmanip.h>

namespace ShaderCompiler {

    Metadata::Metadata(const rapidjson::Value& json) {
        auto sourceFileArray = json["source_files"].GetArray();
        for(const auto& sourceFile : sourceFileArray) {
            std::filesystem::path path = sourceFile.GetString();
            sourceFiles.emplace_back(std::move(path));
        }
        command = json["command"].GetString();
    }

    void Metadata::writeJSON(rapidjson::Document& document) const {
        rapidjson::Value sourceFileArray(rapidjson::kArrayType);
        for(const auto& p : sourceFiles) {
            rapidjson::Value sourceFileKey(Carrot::toString(p.u8string()).c_str(), document.GetAllocator());
            sourceFileArray.PushBack(sourceFileKey, document.GetAllocator());
        }

        document.AddMember("source_files", sourceFileArray, document.GetAllocator());

        rapidjson::Value commandKey(command.c_str(), document.GetAllocator());
        document.AddMember("command", commandKey, document.GetAllocator());
    }


}