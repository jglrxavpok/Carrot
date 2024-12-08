//
// Created by jglrxavpok on 07/12/2024.
//

#pragma once
#include <filesystem>

namespace ShaderCompiler {

    enum class Stage {
        Vertex,
        Fragment,
        Compute,
        Task,
        Mesh,
        RayGen,
        RayMiss,
        RayAnyHit,
        RayClosestHit,
    };

    std::string createCompiledShaderName(const char* shaderFilename, const char* entryPointName);

    int compileShader(const char* basePath, const char* inputFilepath, const char* outputFilepath, Stage stage, std::vector<std::filesystem::path>& dependencies, const char* entryPointName);
}
