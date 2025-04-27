//
// Created by jglrxavpok on 24/11/2021.
//

#include <cstdint>
#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>
#include "FileIncluder.h"
#include <core/data/ShaderMetadata.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/filewritestream.h>
#include <core/utils/stringmanip.h>

// imports from glslang
#include "ShaderCompiler.h"
#include "core/utils/PortabilityHelper.h"

void showUsage() {
    std::cerr <<
        "shadercompiler [base path] [input file] [output file] [stage] (entry point)" << '\n'
        << "\tCompiles a shader and write additional metadata next to the output." << '\n'
        << "\t\t- [base path]: Path to <source folder>/resources/shaders" << '\n'
        << "\t\t- [input file]: Path of file inside <source folder>/resources/shaders to compile" << '\n'
        << "\t\t- [output file]: Path of file inside <build folder>/resources/shaders to compile" << '\n'
        << "\t\t- [stage]: Shader type to add" << '\n'
        << "\t\t- (entry point): Name of entry point, defaults to 'main'" << '\n'
        << '\n'
        << std::endl;
}

int main(int argc, const char** argv) {
    if(argc < 5) {
        std::cerr << "Missing arguments" << std::endl;
        showUsage();
        return -1;
    }

    const char* basePath = argv[1];
    const char* filename = argv[2];
    const char* outFilename = argv[3];
    const char* stageStr = argv[4];
    const char* entryPointName = argc >= 6 ? argv[5] : "main";

    ShaderCompiler::Stage stage = ShaderCompiler::Stage::Fragment;
    if(stricmp(stageStr, "fragment") == 0) {
        stage = ShaderCompiler::Stage::Fragment;
    } else if(stricmp(stageStr, "vertex") == 0) {
        stage = ShaderCompiler::Stage::Vertex;
    } else if(stricmp(stageStr, "rgen") == 0) {
        stage = ShaderCompiler::Stage::RayGen;
    } else if(stricmp(stageStr, "rchit") == 0) {
        stage = ShaderCompiler::Stage::RayClosestHit;
    } else if(stricmp(stageStr, "compute") == 0) {
        stage = ShaderCompiler::Stage::Compute;
    } else if(stricmp(stageStr, "rmiss") == 0) {
        stage = ShaderCompiler::Stage::RayMiss;
    } else if(stricmp(stageStr, "task") == 0) {
        stage = ShaderCompiler::Stage::Task;
    } else if(stricmp(stageStr, "mesh") == 0) {
        stage = ShaderCompiler::Stage::Mesh;
    } else {
        std::cerr << "Invalid stage: " << stageStr << std::endl;
        return -1;
    }
    std::vector<std::filesystem::path> includedFiles;
    int res = ShaderCompiler::compileShader(basePath, filename, outFilename, stage, includedFiles, entryPointName);
    if(res == 0) {
        // depfile (for CMake)
        std::filesystem::path depfilePath = outFilename;
        depfilePath.replace_extension(".spv.d");
        std::wofstream outputFile(depfilePath);

        std::filesystem::path relativeOutput = std::filesystem::relative(outFilename, std::filesystem::current_path());
        outputFile << outFilename << ": ";
        for(const auto& includedFile : includedFiles) {
            std::wstring path = includedFile.wstring();
            // replace separators
            for(std::size_t i = 0; i < path.size(); i++) {
                if(path[i] == L'\\') {
                    path[i] = L'/';
                }
            }
            outputFile << path << " ";
        }
    }
    return res;
}