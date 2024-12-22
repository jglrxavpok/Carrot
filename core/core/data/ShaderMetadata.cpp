//
// Created by jglrxavpok on 27/11/2021.
//

#include "ShaderMetadata.h"
#include <core/utils/stringmanip.h>
#include <core/Macros.h>

namespace ShaderCompiler {

    Metadata::Metadata(const rapidjson::Value& json) {
        auto sourceFileArray = json["source_files"].GetArray();
        for(const auto& sourceFile : sourceFileArray) {
            std::filesystem::path path = sourceFile.GetString();
            sourceFiles.emplace_back(std::move(path));
        }
        auto args = json["command_arguments"].GetArray();
        verify(args.Size() == commandArguments.size(), "Wrong size for command_arguments");
        commandArguments = {
                args[0].GetString(),
                args[1].GetString(),
                args[2].GetString(),
                args[3].GetString(),
                args[4].GetString(),
        };
    }

    void Metadata::writeJSON(rapidjson::Document& document) const {
        rapidjson::Value sourceFileArray(rapidjson::kArrayType);
        for(const auto& p : sourceFiles) {
            rapidjson::Value sourceFileKey(Carrot::toString(p.u8string()).c_str(), document.GetAllocator());
            sourceFileArray.PushBack(sourceFileKey, document.GetAllocator());
        }

        document.AddMember("source_files", sourceFileArray, document.GetAllocator());

        rapidjson::Value commandArgumentsArray(rapidjson::kArrayType);
        for(const auto& arg : commandArguments) {
            rapidjson::Value argument(arg.c_str(), document.GetAllocator());
            commandArgumentsArray.PushBack(argument, document.GetAllocator());
        }
        document.AddMember("command_arguments", commandArgumentsArray, document.GetAllocator());
    }


}