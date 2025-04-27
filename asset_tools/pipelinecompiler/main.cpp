//
// Created by jglrxavpok on 07/12/2024.
//

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <core/Macros.h>
#include <core/io/FileSystemOS.h>
#include <core/utils/stringmanip.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/prettywriter.h>

#include "../shadercompiler/ShaderCompiler.h"


void printUsage() {
    std::cerr << "pipelinecompiler [base path] [output base path] [relative path of pipeline]\n"
                 "Inputs filepaths (pipeline and shader paths) are relative to [base path] and outputs are relative to [output base path]" << std::endl;
}

bool hasShaderCompiler() {
    return Carrot::IO::hasExecutableInWorkingDirectory("shadercompiler");
}

int main(int argc, char** argv) {
    if(!hasShaderCompiler()) {
        std::cerr << "Missing shadercompiler.exe in working dir, won't be able to launch" << std::endl;
        printUsage();
        return -1;
    }

    if(argc != 4) {
        printUsage();
        return -2;
    }

    const std::filesystem::path basePath { argv[1] };
    const std::filesystem::path outputBasePath { argv[2] };
    const std::filesystem::path relativePipelinePath { argv[3] };

    if(!std::filesystem::exists(outputBasePath)) {
        std::filesystem::create_directories(outputBasePath);
    }

    const std::filesystem::path inputFilepath{ basePath / relativePipelinePath };
    std::ifstream stream{ inputFilepath, std::ios::binary | std::ios::in };
    std::string fileContents;

    std::string line;
    while (getline(stream, line)) {
        if (!fileContents.empty()) {
            fileContents += '\n';
        }
        fileContents += line;
    }
    rapidjson::Document d;
    d.Parse(fileContents.data());

    if(d.HasParseError()) {
        std::cerr << rapidjson::GetParseError_En(d.GetParseError()) << std::endl;
        return -3;
    }

    const std::filesystem::path shaderCompilerBasePath = basePath / "resources" / "shaders";
    std::vector<std::filesystem::path> dependencies;
    auto handleShaderRef = [&](const char* memberName, ShaderCompiler::Stage stage) -> bool{
        auto memberIter = d.FindMember(memberName);
        if(memberIter == d.MemberEnd()) {
            return true;
        }

        auto& value = memberIter->value;
        if(!value.IsObject()) {
            std::cerr << memberName << " is not an object: old format not supported" << std::endl;
            return false;
        }

        auto fileIter = value.FindMember("file");
        if(fileIter == value.MemberEnd()) {
            std::cerr << memberName << " is missing 'file' field" << std::endl;
            return false;
        }

        auto entryPointIter = value.FindMember("entry_point");
        if(entryPointIter == value.MemberEnd()) {
            std::cerr << memberName << " is missing 'entryPoint' field" << std::endl;
            return false;
        }

        // make sure pipeline refers to compiled shader
        const std::string shaderPath{ fileIter->value.GetString(), fileIter->value.GetStringLength() };

        // removes file & entry object by a single path (which contains entry point name)
        const std::string compiledShaderPath = ShaderCompiler::createCompiledShaderName(fileIter->value.GetString(), entryPointIter->value.GetString());
        value = rapidjson::Value{ compiledShaderPath.c_str(), d.GetAllocator() };

        const std::filesystem::path shaderAbsoluteInputPath = basePath / shaderPath;
        const std::filesystem::path shaderAbsoluteOutputPath = outputBasePath / compiledShaderPath;

        int result = ShaderCompiler::compileShader(
            shaderCompilerBasePath.string().data(),
            shaderAbsoluteInputPath.string().data(),
            shaderAbsoluteOutputPath.string().data(),
            stage,
            dependencies,
            entryPointIter->value.GetString()
            );

        dependencies.push_back(shaderAbsoluteInputPath);

        if(result != 0) {
            std::cerr << "shadercompiler failed." << std::endl;
            return false;
        }
        return true;
    };

    bool valid = true;
    valid &= handleShaderRef("vertexShader", ShaderCompiler::Stage::Vertex);
    valid &= handleShaderRef("fragmentShader", ShaderCompiler::Stage::Fragment);
    valid &= handleShaderRef("computeShader", ShaderCompiler::Stage::Compute);
    valid &= handleShaderRef("meshShader", ShaderCompiler::Stage::Mesh);
    valid &= handleShaderRef("taskShader", ShaderCompiler::Stage::Task);
    if(!valid) {
        return -4;
    }

    std::filesystem::path outFilename = outputBasePath / relativePipelinePath;
    outFilename.replace_extension(".pipeline");

    auto importIter = d.FindMember("_import");
    if(importIter != d.MemberEnd()) {
        std::filesystem::path importedPath = inputFilepath.parent_path() / importIter->value.GetString();
        dependencies.push_back(importedPath);

        std::filesystem::path importVal { importIter->value.GetString() };
        importVal.replace_extension(".pipeline");
        importIter->value = rapidjson::Value { importVal.string().c_str(), d.GetAllocator() };
    }

    if(!std::filesystem::exists(outFilename.parent_path())) {
        std::filesystem::create_directories(outFilename.parent_path());
    }

    {
        // depfile (for CMake)
        std::filesystem::path depfilePath = outFilename;
        depfilePath.replace_extension(".pipeline.d");
        std::wofstream outputFile(depfilePath);

        outputFile << outFilename.c_str() << ": ";

        auto addDependency = [&](const std::filesystem::path& dependencyPath) {
            std::wstring path = dependencyPath.wstring();
            // replace separators
            for(std::size_t i = 0; i < path.size(); i++) {
                if(path[i] == L'\\') {
                    path[i] = L'/';
                }
            }
            outputFile << path << " ";
        };
        for(const auto& includedFile : dependencies) {
            addDependency(includedFile);
        }
    }

    // write to output file
    {
        FILE *fp = fopen(Carrot::toString(outFilename.u8string()).c_str(), "wb"); // non-Windows use "w"

        char writeBuffer[65536];
        rapidjson::FileWriteStream os(fp, writeBuffer, sizeof(writeBuffer));

        rapidjson::PrettyWriter<rapidjson::FileWriteStream> writer(os);

        d.Accept(writer);
    }

    return 0;
}