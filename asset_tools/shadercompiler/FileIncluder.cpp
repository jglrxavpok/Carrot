//
// Created by jglrxavpok on 24/11/2021.
//

#include "FileIncluder.h"
#include <filesystem>
#include <string>
#include <fstream>

namespace ShaderCompiler {
    static glslang::TShader::Includer::IncludeResult * include(const char* includeFile, const std::filesystem::path& basePath) {
        std::filesystem::path fullPath = basePath / includeFile;

        std::ifstream inputStream(fullPath, std::fstream::binary | std::fstream::ate);
        std::size_t headerLength = inputStream.tellg();
        inputStream.seekg(0);
        char* headerData = reinterpret_cast<char*>(malloc(headerLength));
        inputStream.read(headerData, headerLength);

        return new glslang::TShader::Includer::IncludeResult(fullPath.string(), headerData, headerLength, nullptr);
    }

    glslang::TShader::Includer::IncludeResult *
    FileIncluder::includeSystem(const char *string, const char *string1, size_t size) {
        return include(string, "resources/shaders");
    }

    glslang::TShader::Includer::IncludeResult *
    FileIncluder::includeLocal(const char *string, const char *string1, size_t size) {
        return include(string, std::filesystem::path(string1).parent_path());
    }

    void FileIncluder::releaseInclude(glslang::TShader::Includer::IncludeResult *result) {
        if(!result)
            return;
        free((void *) result->headerData);
        free(result);
    }

    FileIncluder::~FileIncluder() {

    }

}