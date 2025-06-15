//
// Created by jglrxavpok on 14/06/25.
//

#include "GlslCompiler.h"
#include <SPIRV/Logger.h>
#include <SPIRV/SpvTools.h>
#include <SPIRV/GlslangToSpv.h>

#include <array>
#include <fstream>
#include <iostream>
#include <core/utils/Assert.h>

#include "FileIncluder.h"
#include "ShaderCompiler.h"
#include "glslang/Public/ShaderLang.h"
#include "glslang/Public/ResourceLimits.h"

namespace GlslCompiler {
    EShLanguage convertToGLSLang(ShaderCompiler::Stage stage) {
        switch(stage) {
            case ShaderCompiler::Stage::Compute:
                return EShLanguage::EShLangCompute;

            case ShaderCompiler::Stage::Vertex:
                return EShLanguage::EShLangVertex;

            case ShaderCompiler::Stage::Fragment:
                return EShLanguage::EShLangFragment;

            case ShaderCompiler::Stage::Mesh:
                return EShLanguage::EShLangMesh;

            case ShaderCompiler::Stage::Task:
                return EShLanguage::EShLangTask;

            case ShaderCompiler::Stage::RayGen:
                return EShLanguage::EShLangRayGen;

            case ShaderCompiler::Stage::RayMiss:
                return EShLanguage::EShLangMiss;

            case ShaderCompiler::Stage::RayAnyHit:
                return EShLanguage::EShLangAnyHit;

            case ShaderCompiler::Stage::RayClosestHit:
                return EShLanguage::EShLangClosestHit;

            default:
                throw std::invalid_argument("Unknown stage");
        }
    }

    int compileToSpirv(const char* basePath, ShaderCompiler::Stage stageCarrot, const char* entryPointName, std::filesystem::path inputFile, std::vector<std::uint32_t>& spirv, ShaderCompiler::FileIncluder& includer) {
        verify(strcasecmp(entryPointName, ShaderCompiler::InferEntryPointName) != 0, "GLSL compiler cannot infer entry point names");
        EShLanguage stage = convertToGLSLang(stageCarrot);
        const char* stageStr = convertToStr(stageCarrot);

        glslang::TShader shader(stage);

        shader.setEntryPoint("main");
        shader.setSourceEntryPoint(entryPointName);
        shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, glslang::EShTargetVulkan_1_2);
        shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_2);
        shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_5);

        std::ifstream file(inputFile, std::ios::in);

        std::string filecontents;

        std::string line;
        while (getline(file, line)) {
            if (!filecontents.empty()) {
                filecontents += '\n';
            }
            filecontents += line;
        }

        std::string preamble = R"(
    #extension GL_GOOGLE_include_directive : enable
    #extension GL_ARB_separate_shader_objects : enable
    #extension GL_EXT_control_flow_attributes: enable
    #extension GL_EXT_samplerless_texture_functions: enable
    #extension GL_ARB_shader_draw_parameters: enable
    )";
        auto filepath = inputFile.string();
        std::array strs {
            filecontents.c_str(),
        };
        std::array names {
            filepath.c_str(),
        };
        shader.setPreamble(preamble.c_str());
        shader.setStringsWithLengthsAndNames(strs.data(), nullptr, names.data(), strs.size());

        TBuiltInResource Resources = *GetDefaultResources();
        if(!shader.parse(&Resources, 460, false, EShMsgDefault, includer)) {
            std::cerr << "Failed shader compilation. " << shader.getInfoLog() << std::endl;
            return -4;
        }

        glslang::TProgram program;
        program.addShader(&shader);
        if(!program.link(EShMsgDefault)) {
            std::cerr << "Failed shader linking. " << program.getInfoLog() << std::endl;
            return -5;
        }

        auto& shaders = program.getShaders(stage);
        if(shaders.empty()) {
            std::cerr << "No program of type " << stageStr << " has been linked. This should NOT happen!!" << std::endl;
            return -6;
        }

        if(!program.mapIO()) {
            std::cerr << "Failed shader linking (glslang mapIO). " << program.getInfoLog() << std::endl;
            return -7;
        }

        spv::SpvBuildLogger logger;
        glslang::SpvOptions spvOptions;

        // TODO: argument
        spvOptions.generateDebugInfo = true;
        spvOptions.stripDebugInfo = false;

        spvOptions.disableOptimizer = true;
        spvOptions.optimizeSize = false;
        spvOptions.disassemble = false;
        spvOptions.validate = true;
        glslang::GlslangToSpv(*program.getIntermediate(stage), spirv, &logger, &spvOptions);
        return 0;
    }
}