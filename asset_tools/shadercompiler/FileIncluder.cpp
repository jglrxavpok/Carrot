//
// Created by jglrxavpok on 24/11/2021.
//

#include "FileIncluder.h"
#include <string>
#include <fstream>

namespace ShaderCompiler {
    static glslang::TShader::Includer::IncludeResult * include(
            const char* includeFile,
            const std::filesystem::path& basePath,
            std::vector<std::filesystem::path>& includedFiles) {
        std::filesystem::path fullPath = basePath / includeFile;

        if(!std::filesystem::exists(fullPath)) {
            return nullptr;
        }

        std::ifstream inputStream(fullPath, std::fstream::binary | std::fstream::ate);
        std::size_t headerLength = inputStream.tellg();
        inputStream.seekg(0);
        char* headerData = reinterpret_cast<char*>(operator new(headerLength));
        inputStream.read(headerData, headerLength);

        includedFiles.push_back(fullPath);

        return new glslang::TShader::Includer::IncludeResult(fullPath.string(), headerData, headerLength, nullptr);
    }

    glslang::TShader::Includer::IncludeResult *
    FileIncluder::includeSystem(const char *string, const char *string1, size_t size) {
        return include(string, basePath, includedFiles);
    }

    glslang::TShader::Includer::IncludeResult *
    FileIncluder::includeLocal(const char *string, const char *string1, size_t size) {
        return include(string, std::filesystem::path(string1).parent_path(), includedFiles);
    }

    void FileIncluder::releaseInclude(glslang::TShader::Includer::IncludeResult *result) {
        if(!result)
            return;
        operator delete((void *) result->headerData);
        delete result;
    }

    FileIncluder::~FileIncluder() {

    }

}