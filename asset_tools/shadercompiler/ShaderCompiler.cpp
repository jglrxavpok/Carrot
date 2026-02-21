//
// Created by jglrxavpok on 07/12/2024.
//

#include "ShaderCompiler.h"

#include <iostream>
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include <core/data/ShaderMetadata.h>
#include <core/utils/stringmanip.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/prettywriter.h>
#include <spirv_reflect.h>
#include <core/containers/Vector.hpp>
#include <core/io/Logging.hpp>

#include "FileIncluder.h"
#include "GlslCompiler.h"
#include "SlangCompiler.h"

namespace ShaderCompiler {
    const char* convertToStr(ShaderCompiler::Stage stage) {
        switch(stage) {
            case ShaderCompiler::Stage::Compute:
                return "Compute";

            case ShaderCompiler::Stage::Vertex:
                return "Vertex";

            case ShaderCompiler::Stage::Fragment:
                return "Fragment";

            case ShaderCompiler::Stage::Mesh:
                return "Mesh";

            case ShaderCompiler::Stage::Task:
                return "Task";

            case ShaderCompiler::Stage::RayGen:
                return "RayGen";

            case ShaderCompiler::Stage::RayMiss:
                return "Miss";

            case ShaderCompiler::Stage::RayAnyHit:
                return "AnyHit";

            case ShaderCompiler::Stage::RayClosestHit:
                return "ClosestHit";

            default:
                throw std::invalid_argument("Unknown stage");
        }
    }

    std::string createCompiledShaderName(const char* shaderFilename, Stage stage, const char* entryPointName) {
        std::filesystem::path compiled{ shaderFilename };
        compiled += '#';
        compiled += entryPointName;
        compiled += '-';
        compiled += convertToStr(stage);
        compiled += ".spv";
        return compiled.string();
    }

    int compileShader(const char *basePath, const char *inputFilepath, const char *outputFilepath, Stage stageCarrot, std::vector<std::filesystem::path>& includedFiles, const char* entryPointName,
        std::unordered_map<std::string, ShaderCompiler::BindingSlot>& outBindings) {
        if(!glslang::InitializeProcess()) {
            std::cerr << "Failed to setup glslang." << std::endl;
            return -2;
        }

        std::filesystem::path inputFile = inputFilepath;
        std::filesystem::path outputPath = outputFilepath;

        if(!std::filesystem::exists(inputFile)) {
            std::cerr << "File does not exist: " << inputFile.string().c_str() << std::endl;
            return -3;
        }

        if(!std::filesystem::exists(outputPath.parent_path())) {
            std::filesystem::create_directories(outputPath.parent_path());
        }

        const std::filesystem::path extension = inputFile.extension();
        std::vector<std::uint32_t> spirv;
        const char* stageStr = convertToStr(stageCarrot);
        ShaderCompiler::FileIncluder includer{ basePath };
        if (extension == ".glsl") {
            int result = GlslCompiler::compileToSpirv(basePath, stageCarrot, entryPointName, inputFile, spirv, includer);
            if (result != 0) {
                return result;
            }
        } else if (extension == ".slang") {
            int result = SlangCompiler::compileToSpirv(basePath, stageCarrot, entryPointName, inputFile, spirv, includer);
            if (result != 0) {
                return result;
            }
        } else {
            throw std::invalid_argument(Carrot::sprintf("File %s has extension %s which is not supported: only glsl and slang are supported", inputFile.string().c_str(), extension.string().c_str()));
        }

        // runtime metadata file (for hot reload & reflection)
        ShaderCompiler::Metadata metadata{};

        // write reflection data
        {
            spv_reflect::ShaderModule mod{spirv};

            u32 count;
            SpvReflectResult res = mod.EnumerateDescriptorBindings(&count, nullptr);
            verify(res == SpvReflectResult::SPV_REFLECT_RESULT_SUCCESS, "Failed to enumerate");

            Carrot::Vector<SpvReflectDescriptorBinding*> bindings { count };
            res = mod.EnumerateDescriptorBindings(&count, bindings.data());
            verify(res == SpvReflectResult::SPV_REFLECT_RESULT_SUCCESS, "Failed to enumerate");

            for (u32 index = 0; index < count; index++) {
                ShaderCompiler::BindingSlot& slot = outBindings[bindings[index]->name];
                slot.setID = bindings[index]->set;
                slot.bindingID = bindings[index]->binding;
                slot.type = static_cast<VkDescriptorType>(bindings[index]->descriptor_type);
            }
        }

        {
            std::ofstream outputFile(outputPath, std::ios::binary);
            outputFile.write(reinterpret_cast<const char *>(spirv.data()), spirv.size() * sizeof(std::uint32_t));
        }

        // hot reload support
        {
            for (const auto& includedFile: includer.includedFiles) {
                metadata.sourceFiles.push_back(std::filesystem::absolute(includedFile));

                includedFiles.push_back(includedFile);
            }
            metadata.sourceFiles.push_back(std::filesystem::absolute(inputFile));

            metadata.commandArguments[0] = basePath;
            metadata.commandArguments[1] = inputFilepath;
            metadata.commandArguments[2] = outputFilepath;
            metadata.commandArguments[3] = stageStr;
            metadata.commandArguments[4] = entryPointName;

            auto metadataPath = outputPath;
            metadataPath.replace_extension(".meta.json");
            FILE *fp = fopen(Carrot::toString(metadataPath.u8string()).c_str(), "wb"); // non-Windows use "w"

            char writeBuffer[65536];
            rapidjson::FileWriteStream os(fp, writeBuffer, sizeof(writeBuffer));

            rapidjson::PrettyWriter<rapidjson::FileWriteStream> writer(os);

            rapidjson::Document document;
            document.SetObject();

            metadata.writeJSON(document);

            document.Accept(writer);
            fclose(fp);
        }

        return 0;
    }

}
