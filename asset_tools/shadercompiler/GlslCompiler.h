//
// Created by jglrxavpok on 14/06/25.
//

#pragma once
#include "ShaderCompiler.h"

namespace ShaderCompiler {
    class FileIncluder;
}

namespace GlslCompiler {
    int compileToSpirv(const char* basePath, ShaderCompiler::Stage stageCarrot, const char* entryPointName, std::filesystem::path inputFile, std::vector<std::uint32_t>& spirv, ShaderCompiler::FileIncluder& includer);
}
