//
// Created by jglrxavpok on 14/06/25.
//

#include "SlangCompiler.h"

#include <iostream>
#include <slang-com-ptr.h>
#include <core/Macros.h>
#include <slang.h>
#include <core/utils/PortabilityHelper.h>

#include "FileIncluder.h"

using namespace slang;
namespace SlangCompiler {
    SlangStage toSlangStage(ShaderCompiler::Stage stageCarrot) {
        switch (stageCarrot) {
            case ShaderCompiler::Stage::Vertex:
                return SlangStage::SLANG_STAGE_VERTEX;
            case ShaderCompiler::Stage::Fragment:
                return SlangStage::SLANG_STAGE_PIXEL;
            case ShaderCompiler::Stage::Compute:
                return SlangStage::SLANG_STAGE_COMPUTE;
            case ShaderCompiler::Stage::Task:
                return SlangStage::SLANG_STAGE_AMPLIFICATION;
            case ShaderCompiler::Stage::Mesh:
                return SlangStage::SLANG_STAGE_MESH;
            case ShaderCompiler::Stage::RayGen:
                return SlangStage::SLANG_STAGE_RAY_GENERATION;
            case ShaderCompiler::Stage::RayMiss:
                return SlangStage::SLANG_STAGE_MISS;
            case ShaderCompiler::Stage::RayAnyHit:
                return SlangStage::SLANG_STAGE_ANY_HIT;
            case ShaderCompiler::Stage::RayClosestHit:
                return SlangStage::SLANG_STAGE_CLOSEST_HIT;

            default:
                TODO; // missing case
        }
    }

    int compileToSpirv(const char* basePath, ShaderCompiler::Stage stageCarrot, const char* entryPointName, std::filesystem::path inputFile, std::vector<std::uint32_t>& spirv, ShaderCompiler::FileIncluder& includer) {
        const bool inferEntryPointName = stricmp(entryPointName, ShaderCompiler::InferEntryPointName) == 0;

        // create global session. This probably wastes some time recreating it for each compiled shader, but each shader is considered independent from a build-system point-of-view
        Slang::ComPtr<IGlobalSession> globalSession;
        SlangGlobalSessionDesc desc = {};
        SlangResult result = createGlobalSession(&desc, globalSession.writeRef());
        if (result < 0) {
            std::cerr << "Slang createGlobalSession failed with error " << result << std::endl;
            return result;
        }

        std::vector<CompilerOptionEntry> compilerOptions;
        if (inferEntryPointName) {
            compilerOptions.emplace_back(CompilerOptionEntry {
                .name = CompilerOptionName::Stage,
                .value = CompilerOptionValue {
                    .kind = CompilerOptionValueKind::Int,
                    .intValue0 = (int)toSlangStage(stageCarrot)
                }
            });
        } else {
            compilerOptions.emplace_back(CompilerOptionEntry {
                .name = CompilerOptionName::EntryPointName,
                .value = CompilerOptionValue {
                    .kind = CompilerOptionValueKind::String,
                    .stringValue0 = entryPointName
                }
            });
        }
        compilerOptions.emplace_back(CompilerOptionEntry {
            .name = CompilerOptionName::Profile,
            .value = CompilerOptionValue {
                .kind = CompilerOptionValueKind::Int,
                .intValue0 = static_cast<int>(globalSession->findProfile("glsl_460"))
            }
        });

        // Without this, Slang does not generate AtomicExchange ?
        compilerOptions.emplace_back(CompilerOptionEntry {
            .name = CompilerOptionName::Capability,
            .value = CompilerOptionValue {
                .kind = CompilerOptionValueKind::Int,
                .intValue0 = static_cast<int>(globalSession->findCapability("spirv_1_6"))
            }
        });
        compilerOptions.emplace_back(CompilerOptionEntry {
            .name = CompilerOptionName::DebugInformation,
            .value = CompilerOptionValue {
                .kind = CompilerOptionValueKind::Int,
                .intValue0 = SLANG_DEBUG_INFO_LEVEL_MAXIMAL
            }
        });

        // TODO: This is currently required because Carrot binds to unused parameters, which Slang removes.
        //  This creates GPU hangs/crashes on RADV :c
        compilerOptions.emplace_back(CompilerOptionEntry {
            .name = CompilerOptionName::Optimization,
            .value = CompilerOptionValue {
                .kind = CompilerOptionValueKind::Int,
                .intValue0 = SLANG_OPTIMIZATION_LEVEL_NONE
            }
        });
        compilerOptions.emplace_back(CompilerOptionEntry {
            .name = CompilerOptionName::PreserveParameters,
            .value = CompilerOptionValue {
                .kind = CompilerOptionValueKind::Int,
                .intValue0 = 1
            }
        });

        TargetDesc target{};
        target.compilerOptionEntries = compilerOptions.data();
        target.compilerOptionEntryCount = compilerOptions.size();
        target.format = SlangCompileTarget::SLANG_SPIRV;

        const char* defaultSearchPaths[] = { basePath };

        SessionDesc sessionDesc{
            .targets = &target,
            .targetCount = 1,

            .searchPaths = defaultSearchPaths,
            .searchPathCount = 1,
        };

        Slang::ComPtr<ISession> session;
        result = globalSession->createSession(sessionDesc, session.writeRef());
        if (result < 0) {
            std::cerr << "Slang createSession failed with error " << result << std::endl;
            return result;
        }

        const std::string inputFilename = inputFile.stem().string();

        Slang::ComPtr<SlangCompileRequest> request;
        result = session->createCompileRequest(request.writeRef());
        if (result < 0) {
            std::cerr << "Slang createCompileRequest failed with error " << result << std::endl;
            return result;
        }

        int unitID = request->addTranslationUnit(SlangSourceLanguage::SLANG_SOURCE_LANGUAGE_SLANG, inputFilename.c_str());
        request->addTranslationUnitSourceFile(unitID, inputFile.string().c_str());
        // .slang files can contain multiple entry points, so based on the stage we can find the correct one
        result = request->compile();
        if (result < 0) {
            std::cerr << "Slang compile failed with error " << result << std::endl;
            std::cerr << request->getDiagnosticOutput() << std::endl;
            return result;
        }

        Slang::ComPtr<IComponentType> programWithEntryPoints;
        result = request->getProgramWithEntryPoints(programWithEntryPoints.writeRef());
        if (result < 0) {
            std::cerr << "Slang getProgramWithEntryPoints failed with error " << result << std::endl;
            std::cerr << request->getDiagnosticOutput() << std::endl;
            return result;
        }
        Slang::ComPtr<IComponentType> linkedProgram;
        programWithEntryPoints->link(linkedProgram.writeRef(), nullptr);

        SlangStage expectedStage = toSlangStage(stageCarrot);
        ProgramLayout* layout = linkedProgram->getLayout();
        Slang::ComPtr<IBlob> code;
        for (SlangUInt entryPointIndex = 0; entryPointIndex < layout->getEntryPointCount(); entryPointIndex++) {
            EntryPointReflection* entryPoint = layout->getEntryPointByIndex(entryPointIndex);
            if (entryPoint->getStage() == expectedStage) {
                if (inferEntryPointName) {
                    linkedProgram->getEntryPointCode(entryPointIndex, 0, code.writeRef());
                    break;
                } else {
                    if (stricmp(entryPoint->getName(), entryPointName) == 0) {
                        linkedProgram->getEntryPointCode(entryPointIndex, 0, code.writeRef());
                        break;
                    }
                }
            }
        }

        if (!code) {
            std::cerr << "No entry point correspond to stage " << ShaderCompiler::convertToStr(stageCarrot) << std::endl;
            return -1;
        }

        // gather dependencies to create files required for hot reloading and CMake recompilation
        int dependencyCount = request->getDependencyFileCount();
        includer.includedFiles.reserve(dependencyCount);
        for (int dependencyIndex = 0; dependencyIndex < dependencyCount; dependencyIndex++) {
            includer.includedFiles.emplace_back(request->getDependencyFilePath(dependencyIndex));
        }

        // extract SPIR-V
        std::size_t bytecodeSize = code->getBufferSize();
        const void* pBytecode = code->getBufferPointer();
        verify(bytecodeSize % sizeof(std::uint32_t) == 0, "Returned bytecode is not multiple of sizeof(u32) ?");
        spirv.resize(bytecodeSize / sizeof(std::uint32_t));
        memcpy(spirv.data(), pBytecode, bytecodeSize);

        return 0;
    }
}
