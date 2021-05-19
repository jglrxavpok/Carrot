//
// Created by jglrxavpok on 19/05/2021.
//

#include "ParticleShaderGenerator.h"

namespace Tools {
    using namespace spv;

    ParticleShaderGenerator::ParticleShaderGenerator() {

    }

    std::vector<uint32_t> ParticleShaderGenerator::compileToSPIRV(const std::vector<std::shared_ptr<Carrot::Expression>>& expressions) {
        SpvBuildLogger logger;
        Builder builder(spv::SpvVersion::Spv_1_5, 0, &logger);

        builder.addCapability(spv::CapabilityShader);
        builder.import("GLSL.std.450");
        builder.setMemoryModel(spv::AddressingModelLogical, spv::MemoryModelGLSL450);

        // Carrot::ParticleStatistics
        auto particleStats = builder.makeStructType(std::vector<Id>{builder.makeFloatType(32), builder.makeUintType(32)}, "ParticleStatistics");
        builder.addMemberName(particleStats, 0, "deltaTime");
        builder.addMemberName(particleStats, 1, "particleCount");

        spv::Block* functionBlock;

        auto vec3Type = builder.makeVectorType(builder.makeFloatType(32), 3);
        auto vec3InputPointer = builder.makePointer(spv::StorageClassInput, vec3Type);
        std::vector<std::vector<Decoration>> precisions{};

        auto globalInvocationId = builder.createVariable(spv::NoPrecision,
                                                         spv::StorageClassInput,
                                                         builder.makeVectorType(builder.makeUintType(32), 3),
                                                         "gl_GlobalInvocationID");
        auto descriptorSet = builder.getUniqueId();
        builder.addName(descriptorSet, "DescriptorSet0");
        builder.addDecoration(descriptorSet, spv::DecorationDescriptorSet, 0);
        builder.addDecoration(descriptorSet, spv::DecorationBinding, 1);

        auto gl_WorkGroupSize = builder.makeCompositeConstant(builder.makeVectorType(builder.makeUintType(32), 3),
                                                              std::vector{
                                                                      builder.makeIntConstant(1024),
                                                                      builder.makeIntConstant(1),
                                                                      builder.makeIntConstant(1)
                                                              });
        builder.addDecoration(gl_WorkGroupSize, spv::DecorationBuiltIn, spv::BuiltIn::BuiltInWorkgroupSize);
        {


            builder.addDecoration(globalInvocationId, spv::DecorationBuiltIn, spv::BuiltInGlobalInvocationId);
            Function* mainFunction = builder.makeFunctionEntry(spv::NoPrecision, builder.makeVoidType(), "main", std::vector{builder.makeVoidType()}, precisions, &functionBlock);

            builder.leaveFunction();
            auto entryPoint = builder.addEntryPoint(spv::ExecutionModelGLCompute, mainFunction, "main");
            entryPoint->addIdOperand(globalInvocationId);
            entryPoint->addIdOperand(particleStats);
            entryPoint->addIdOperand(descriptorSet);

            builder.addExecutionMode(mainFunction, spv::ExecutionModeLocalSize, 1024, 1, 1);
        }

        std::vector<uint32_t> output;
        builder.dump(output);

        std::cout << "SpvBuilder output:\n";
        std::cerr << logger.getAllMessages();

        return std::move(output);
    }
}