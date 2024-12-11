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
#include <SPIRV/Logger.h>
#include <SPIRV/SpvTools.h>
#include <SPIRV/GlslangToSpv.h>

#include "FileIncluder.h"
#include "glslang/Public/ShaderLang.h"
#include "glslang/Public/ResourceLimits.h"

namespace ShaderCompiler {

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

    std::string createCompiledShaderName(const char* shaderFilename, const char* entryPointName) {
        std::filesystem::path compiled{ shaderFilename };
        compiled += '#';
        compiled += entryPointName;
        compiled += ".spv";
        return compiled.string();
    }

    int compileShader(const char *basePath, const char *inputFilepath, const char *outputFilepath, Stage stageCarrot, std::vector<std::filesystem::path>& includedFiles, const char* entryPointName) {
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

        ShaderCompiler::FileIncluder includer { basePath };
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

        std::vector<std::uint32_t> spirv;
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

        {
            std::ofstream outputFile(outputPath, std::ios::binary);
            outputFile.write(reinterpret_cast<const char *>(spirv.data()), spirv.size() * sizeof(std::uint32_t));
        }

        // runtime metadata file (for hot reload)
        {
            ShaderCompiler::Metadata metadata;
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