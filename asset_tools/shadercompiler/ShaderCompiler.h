//
// Created by jglrxavpok on 07/12/2024.
//

#pragma once
#include <filesystem>
#include <unordered_map>
#include <vector>
#include <core/data/ShaderMetadata.h>

namespace ShaderCompiler {
    inline const char* InferEntryPointName = "(inferred)";

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

    const char* convertToStr(ShaderCompiler::Stage stage);
    std::string createCompiledShaderName(const char* shaderFilename, Stage stage, const char* entryPointName);

    int compileShader(const char* basePath, const char* inputFilepath, const char* outputFilepath, Stage stage, std::vector<std::filesystem::path>& dependencies, const char* entryPointName, std::unordered_map<std::string, ShaderCompiler::BindingSlot>& outBindings);
}
