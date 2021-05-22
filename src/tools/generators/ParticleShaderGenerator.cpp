//
// Created by jglrxavpok on 19/05/2021.
//

#include "ParticleShaderGenerator.h"
#include "engine/expressions/ExpressionVisitor.h"
#include <stack>
#include <utility>
#include "tools/nodes/VariableNodes.h"
#include "tools/nodes/TerminalNodes.h"

namespace Tools {
    using namespace spv;

    struct Variables {

        Id position;
        Id life;
        Id velocity;
        Id size;
        Id particleIndex;
        Id emissionID;

        union {
            struct {
                Id deltaTime;
            } compute;

            struct {
                Id particleColor;
            } fragment;
        };

        ParticleShaderMode shaderMode;
    };

    class SPIRVisitor: public Carrot::ExpressionVisitor {
    public:
        inline explicit SPIRVisitor(spv::Builder& builder, Variables vars): builder(builder), vars(vars) {}

    public:
        void visitConstant(Carrot::ConstantExpression& expression) override {
            float value = expression.getValue();
            ids.push(builder.makeFloatConstant(value));
        }

        void visitGetVariable(Carrot::GetVariableExpression& expression) override {
            const auto& var = expression.getVariableName();

            if(var == VariableNode::getInternalName(VariableNodeType::GetSize)) {
                ids.push(builder.createLoad(vars.size, spv::NoPrecision));
            } else if(var == VariableNode::getInternalName(VariableNodeType::GetVelocity)) {
                auto access = builder.createAccessChain(spv::StorageClassStorageBuffer, vars.velocity, {builder.makeIntConstant(expression.getSubIndex())});
                ids.push(builder.createLoad(access, spv::NoPrecision));
            } else if(var == VariableNode::getInternalName(VariableNodeType::GetLife)) {
                ids.push(vars.life);
            } else if(var == VariableNode::getInternalName(VariableNodeType::GetParticleIndex)) {
                ids.push(vars.particleIndex);
            } else if(var == VariableNode::getInternalName(VariableNodeType::GetEmissionID)) {
                ids.push(builder.createLoad(vars.emissionID, spv::NoPrecision));
            } else if(var == VariableNode::getInternalName(VariableNodeType::GetPosition)) {
                auto access = builder.createAccessChain(spv::StorageClassStorageBuffer, vars.position,
                                                        {builder.makeIntConstant(expression.getSubIndex())});
                ids.push(builder.createLoad(access, spv::NoPrecision));
            }
        // === COMPUTE ===
            else if(var == VariableNode::getInternalName(VariableNodeType::GetDeltaTime)) {
                assert(vars.shaderMode == ParticleShaderMode::Compute);
                ids.push(vars.compute.deltaTime);
            }
        // === FRAGMENT ===

        // ================
            else {
                throw std::runtime_error("Unsupported variable type: "+var);
            }
        }

        void visitSetVariable(Carrot::SetVariableExpression& expression) override {
            visit(expression.getValue());

            const auto& var = expression.getVariableName();

            if(var == TerminalNode::getInternalName(TerminalNodeType::SetSize)) {
                builder.createStore(popID(), vars.size);
            } else if(var == TerminalNode::getInternalName(TerminalNodeType::SetVelocity)) {
                auto access = builder.createAccessChain(spv::StorageClassStorageBuffer, vars.velocity,
                                                        {builder.makeIntConstant(expression.getSubIndex())});
                builder.createStore(popID(), access);
            }
        // === COMPUTE ===

        // === FRAGMENT ===
            else if(var == TerminalNode::getInternalName(TerminalNodeType::SetOutputColor)) {
                assert(vars.shaderMode == ParticleShaderMode::Fragment);
                auto access = builder.createAccessChain(spv::StorageClassFunction, vars.fragment.particleColor,
                                                        {builder.makeIntConstant(expression.getSubIndex())});
                builder.createStore(popID(), access);
            }
        // ================
            else {
                throw std::runtime_error("Unsupported variable type: "+var);
            }
        }

        void visitAdd(Carrot::AddExpression& expression) override {
            visitBinOp(spv::OpFAdd, expression.getOperand1(), expression.getOperand2());
        }

        void visitSub(Carrot::SubExpression& expression) override {
            visitBinOp(spv::OpFSub, expression.getOperand1(), expression.getOperand2());
        }

        void visitMult(Carrot::MultExpression& expression) override {
            visitBinOp(spv::OpFMul, expression.getOperand1(), expression.getOperand2());
        }

        void visitDiv(Carrot::DivExpression& expression) override {
            visitBinOp(spv::OpFDiv, expression.getOperand1(), expression.getOperand2());
        }

        void visitCompound(Carrot::CompoundExpression& expression) override {
            for(const auto& e : expression.getSubExpressions()) {
                if(e != nullptr)
                    visit(e);
            }
        }

        void visitMod(Carrot::ModExpression& expression) override {
            visitBinOp(spv::OpFMod, expression.getOperand1(), expression.getOperand2());
        }

    private:
        spv::Builder& builder;
        std::stack<Id> ids;
        Variables vars;

        void visitBinOp(spv::Op op, std::shared_ptr<Carrot::Expression> left, std::shared_ptr<Carrot::Expression> right) {
            visit(left);
            visit(right);
            auto rightID = popID();
            auto leftID = popID();

            auto leftType = builder.getTypeId(leftID);
            auto rightType = builder.getTypeId(rightID);
            if(builder.getMostBasicTypeClass(leftType) != builder.getMostBasicTypeClass(rightType)
            || builder.getNumTypeComponents(leftType) != builder.getNumTypeComponents(rightType)) {
                throw std::runtime_error("Cannot operate on operands of different types");
            }

            auto returnType = leftType;

            ids.push(builder.createBinOp(op, returnType, leftID, rightID));
        }

        Id popID() {
            auto id = ids.top();
            ids.pop();
            return id;
        }
    };

    ParticleShaderGenerator::ParticleShaderGenerator(ParticleShaderMode shaderMode, const std::string& projectName): shaderMode(shaderMode), projectName(projectName) {

    }

    std::vector<uint32_t> ParticleShaderGenerator::compileToSPIRV(const std::vector<std::shared_ptr<Carrot::Expression>>& expressions) {
        SpvBuildLogger logger;
        Builder builder(spv::SpvVersion::Spv_1_5, 0, &logger);

        builder.setSource(spv::SourceLanguageGLSL, 450);
        builder.setSourceFile(projectName);
        builder.addCapability(spv::CapabilityShader);
        builder.import("GLSL.std.450");
        builder.setMemoryModel(spv::AddressingModelLogical, spv::MemoryModelGLSL450);

        auto uint32Type = builder.makeUintType(32);
        auto float32Type = builder.makeFloatType(32);
        auto vec3Type = builder.makeVectorType(float32Type, 3);
        /*
         layout(constant_id = 0) const uint MAX_PARTICLE_COUNT = 1000;

        struct Particle {
            vec3 position;
            float life;
            vec3 velocity;
            float size;

            uint id;
        };

        layout(set = 0, binding = 1) buffer Particles {
            Particle particles[MAX_PARTICLE_COUNT];
        };
         */
        auto particleType = builder.makeStructType(std::vector<Id>{
                vec3Type,
                float32Type,
                vec3Type,
                float32Type,

                uint32Type
        }, "Particle");
        builder.addMemberName(particleType, 0, "position");
        builder.addMemberName(particleType, 1, "life");
        builder.addMemberName(particleType, 2, "velocity");
        builder.addMemberName(particleType, 3, "size");
        builder.addMemberName(particleType, 4, "id");

        builder.addMemberDecoration(particleType, 0, spv::DecorationOffset, 0);
        builder.addMemberDecoration(particleType, 1, spv::DecorationOffset, 12);
        builder.addMemberDecoration(particleType, 2, spv::DecorationOffset, 16);
        builder.addMemberDecoration(particleType, 3, spv::DecorationOffset, 28);
        builder.addMemberDecoration(particleType, 4, spv::DecorationOffset, 32);

        auto maxParticleCount = builder.makeIntConstant(1000, true);
        builder.addName(maxParticleCount, "MAX_PARTICLE_COUNT");
        builder.addDecoration(maxParticleCount, spv::DecorationSpecId, 0);

        auto particleArrayType = builder.makeArrayType(particleType, maxParticleCount, 0);
        builder.addDecoration(particleArrayType, spv::DecorationArrayStride, 48);
        auto storageBufferParticles = builder.makeStructType(std::vector<Id>{particleArrayType}, "Particles");
        builder.addMemberName(storageBufferParticles, 0, "particles");
        builder.addDecoration(storageBufferParticles, spv::DecorationBlock);
        builder.addMemberDecoration(storageBufferParticles, 0, spv::DecorationOffset, 0);

        auto descriptorSet = builder.createVariable(spv::NoPrecision, spv::StorageClassStorageBuffer, storageBufferParticles);
        builder.addName(descriptorSet, "DescriptorSet0");
        builder.addDecoration(descriptorSet, spv::DecorationDescriptorSet, 0);
        builder.addDecoration(descriptorSet, spv::DecorationBinding, 1);

        switch(shaderMode) {
            case ParticleShaderMode::Compute:
                generateCompute(builder, descriptorSet, expressions);
                break;

            case ParticleShaderMode::Fragment:
                generateFragment(builder, descriptorSet, expressions);
                break;
        }

        std::vector<uint32_t> output;
        builder.dump(output);

        std::cout << "SpvBuilder output:\n";
        std::cerr << logger.getAllMessages();
        std::cout << "\n=== END of SpvBuilder output ===";

        return std::move(output);
    }

    void ParticleShaderGenerator::generateFragment(spv::Builder& builder, spv::Id descriptorSet, const std::vector<std::shared_ptr<Carrot::Expression>>& expressions) {
        spv::Block* functionBlock;
        Function* mainFunction = builder.makeFunctionEntry(spv::NoPrecision, builder.makeVoidType(), "main", {}, {}, &functionBlock);

        auto uint32Type = builder.makeUintType(32);
        auto float32Type = builder.makeFloatType(32);
        auto vec3Type = builder.makeVectorType(float32Type, 3);
        auto vec4Type = builder.makeVectorType(float32Type, 4);

        /*
layout(location = 0) out vec4 outColor;
layout(location = 1) out vec3 outViewPosition;
layout(location = 2) out vec3 outNormal;
layout(location = 3) out uint outIntProperties;

layout(location = 0) in flat uint particleIndex;
         */
        auto outColor = builder.createVariable(spv::NoPrecision, spv::StorageClassOutput, vec4Type);
        builder.addName(outColor, "outColor");
        builder.addDecoration(outColor, spv::DecorationLocation, 0);

        auto outViewPosition = builder.createVariable(spv::NoPrecision, spv::StorageClassOutput, vec3Type);
        builder.addName(outViewPosition, "outViewPosition");
        builder.addDecoration(outViewPosition, spv::DecorationLocation, 1);

        auto outNormal = builder.createVariable(spv::NoPrecision, spv::StorageClassOutput, vec3Type);
        builder.addName(outNormal, "outNormal");
        builder.addDecoration(outNormal, spv::DecorationLocation, 2);

        auto outIntProperties = builder.createVariable(spv::NoPrecision, spv::StorageClassOutput, uint32Type);
        builder.addName(outIntProperties, "outIntProperties");
        builder.addDecoration(outIntProperties, spv::DecorationLocation, 3);

        auto inParticleIndex = builder.createVariable(spv::NoPrecision, spv::StorageClassInput, uint32Type);
        builder.addName(inParticleIndex, "inParticleIndex");
        builder.addDecoration(inParticleIndex, spv::DecorationLocation, 0);
        builder.addDecoration(inParticleIndex, spv::DecorationFlat);

        auto varParticleIndex = builder.createVariable(spv::NoPrecision, spv::StorageClassFunction, uint32Type);
        auto tmpParticleIndex = builder.createLoad(inParticleIndex, spv::NoPrecision);
        builder.createStore(tmpParticleIndex, varParticleIndex);

        auto particleIndex = builder.createLoad(varParticleIndex, spv::NoPrecision);
        auto velocityAccess = builder.createAccessChain(spv::StorageClassStorageBuffer, descriptorSet, {builder.makeIntConstant(0), particleIndex, builder.makeIntConstant(2)});
        auto positionAccess = builder.createAccessChain(spv::StorageClassStorageBuffer, descriptorSet, {builder.makeIntConstant(0), particleIndex, builder.makeIntConstant(0)});
        auto lifeAccess = builder.createAccessChain(spv::StorageClassStorageBuffer, descriptorSet, {builder.makeIntConstant(0), particleIndex, builder.makeIntConstant(1)});
        auto sizeAccess = builder.createAccessChain(spv::StorageClassStorageBuffer, descriptorSet, {builder.makeIntConstant(0), particleIndex, builder.makeIntConstant(3)});

        auto varEmissionID = builder.createVariable(spv::NoPrecision, spv::StorageClassFunction, uint32Type);
        auto emissionIDAccess = builder.createAccessChain(spv::StorageClassStorageBuffer, descriptorSet, {builder.makeIntConstant(0), particleIndex, builder.makeIntConstant(4)});
        builder.createStore(builder.createLoad(emissionIDAccess, spv::NoPrecision), varEmissionID);
        auto emissionID = builder.createLoad(varEmissionID, spv::NoPrecision);

        auto castParticleID = builder.createUnaryOp(spv::OpConvertUToF, float32Type, particleIndex);

        auto varFinalColor = builder.createVariable(spv::NoPrecision, spv::StorageClassFunction, vec4Type);
        auto float1 = builder.makeFloatConstant(1.0f);
        builder.createStore(builder.createCompositeConstruct(vec4Type, {float1, float1, float1, float1}), varFinalColor);

        // user-specific code
        Variables vars{};
        vars.shaderMode = shaderMode;
        vars.fragment.particleColor = varFinalColor;
        vars.emissionID = emissionID;
        vars.position = positionAccess;
        vars.life = builder.createLoad(lifeAccess, spv::NoPrecision);
        vars.velocity = velocityAccess;
        vars.size = sizeAccess;
        vars.particleIndex = castParticleID;
        for(auto& expr : expressions) {
            builder.getStringId(expr->toString());

            SPIRVisitor exprBuilder(builder, vars);
            exprBuilder.visit(expr);
        }

        builder.createStore(builder.createLoad(varFinalColor, spv::NoPrecision), outColor);
        builder.createStore(builder.makeUintConstant(0), outIntProperties);

        builder.leaveFunction();
        auto entryPoint = builder.addEntryPoint(spv::ExecutionModelFragment, mainFunction, "main");
        entryPoint->addIdOperand(inParticleIndex);
        entryPoint->addIdOperand(outColor);
        entryPoint->addIdOperand(outViewPosition);
        entryPoint->addIdOperand(outNormal);
        entryPoint->addIdOperand(outIntProperties);

        builder.addExecutionMode(mainFunction, spv::ExecutionModeOriginUpperLeft);
    }

    void ParticleShaderGenerator::generateCompute(spv::Builder& builder, spv::Id descriptorSet, const std::vector<std::shared_ptr<Carrot::Expression>>& expressions) {
        spv::Block* functionBlock;
        Function* mainFunction = builder.makeFunctionEntry(spv::NoPrecision, builder.makeVoidType(), "main", {}, {}, &functionBlock);

        auto uint32Type = builder.makeUintType(32);
        auto float32Type = builder.makeFloatType(32);
        auto vec3Type = builder.makeVectorType(float32Type, 3);

        // Carrot::ParticleStatistics
        auto particleStatsType = builder.makeStructType(std::vector<Id>{builder.makeFloatType(32), builder.makeUintType(32)}, "ParticleStatistics");
        builder.addMemberName(particleStatsType, 0, "deltaTime");
        builder.addMemberName(particleStatsType, 1, "particleCount");

        builder.addDecoration(particleStatsType, spv::DecorationBlock);

        builder.addMemberDecoration(particleStatsType, 0, spv::DecorationOffset, 0);
        builder.addMemberDecoration(particleStatsType, 1, spv::DecorationOffset, 4);

        auto particleStats = builder.createVariable(spv::NoPrecision, spv::StorageClassStorageBuffer, particleStatsType);
        builder.addDecoration(particleStats, spv::DecorationDescriptorSet, 0);
        builder.addDecoration(particleStats, spv::DecorationBinding, 0);
        builder.addName(particleStats, "particleStats");

        auto globalInvocationId = builder.createVariable(spv::NoPrecision,
                                                         spv::StorageClassInput,
                                                         builder.makeVectorType(builder.makeUintType(32), 3),
                                                         "gl_GlobalInvocationID");
        builder.addDecoration(globalInvocationId, spv::DecorationBuiltIn, spv::BuiltInGlobalInvocationId);


        auto gl_WorkGroupSize = builder.makeCompositeConstant(builder.makeVectorType(builder.makeUintType(32), 3),
                                                              std::vector{
                                                                      builder.makeIntConstant(1024),
                                                                      builder.makeIntConstant(1),
                                                                      builder.makeIntConstant(1)
                                                              });
        builder.addDecoration(gl_WorkGroupSize, spv::DecorationBuiltIn, spv::BuiltIn::BuiltInWorkgroupSize);

        auto dtAccess = builder.createAccessChain(spv::StorageClassStorageBuffer, particleStats, std::vector<Id>{builder.makeIntConstant(0)});

        auto varParticleIndex = builder.createVariable(spv::NoPrecision, spv::StorageClassFunction, uint32Type);
        auto globalInvocationAccess = builder.createAccessChain(spv::StorageClassInput, globalInvocationId, std::vector<Id>{builder.makeUintConstant(0)});
        auto tmpParticleIndex = builder.createLoad(globalInvocationAccess, spv::NoPrecision);
        builder.createStore(tmpParticleIndex, varParticleIndex);

        auto particleIndex = builder.createLoad(varParticleIndex, spv::NoPrecision);

        auto varEmissionID = builder.createVariable(spv::NoPrecision, spv::StorageClassFunction, uint32Type);
        auto emissionIDAccess = builder.createAccessChain(spv::StorageClassStorageBuffer, descriptorSet, {builder.makeIntConstant(0), particleIndex, builder.makeIntConstant(4)});
        builder.createStore(emissionIDAccess, varEmissionID);
        auto emissionID = builder.createLoad(varEmissionID, spv::NoPrecision);

        auto velocityAccess = builder.createAccessChain(spv::StorageClassStorageBuffer, descriptorSet, {builder.makeIntConstant(0), particleIndex, builder.makeIntConstant(2)});
        auto positionAccess = builder.createAccessChain(spv::StorageClassStorageBuffer, descriptorSet, {builder.makeIntConstant(0), particleIndex, builder.makeIntConstant(0)});
        auto lifeAccess = builder.createAccessChain(spv::StorageClassStorageBuffer, descriptorSet, {builder.makeIntConstant(0), particleIndex, builder.makeIntConstant(1)});
        auto sizeAccess = builder.createAccessChain(spv::StorageClassStorageBuffer, descriptorSet, {builder.makeIntConstant(0), particleIndex, builder.makeIntConstant(3)});

        auto varDT = builder.createVariable(spv::NoPrecision, spv::StorageClassFunction, float32Type);
        builder.addName(varDT, "dt");

        auto particleCountAccess = builder.createAccessChain(spv::StorageClassStorageBuffer, particleStats, std::vector<Id>{builder.makeIntConstant(1)});
        auto particleCount = builder.createLoad(particleCountAccess, spv::NoPrecision);


        // Generate: if(particleIndex >= totalCount)
        {
            auto shouldContinue = builder.createBinOp(spv::OpUGreaterThanEqual, builder.makeBoolType(), particleIndex, particleCount);
            spv::Builder::If ifValidIndex(shouldContinue, spv::SelectionControlMaskNone, builder);
            builder.makeReturn(false);
            ifValidIndex.makeEndIf();
        }

        auto dtTmp = builder.createLoad(dtAccess, spv::NoPrecision);
        builder.createStore(dtTmp, varDT);

        // update position
        auto dt = builder.createLoad(varDT, spv::NoPrecision);

        auto velocity = builder.createLoad(velocityAccess, spv::NoPrecision);
        auto velocityTimesDeltaTime = builder.createBinOp(spv::OpVectorTimesScalar, vec3Type, velocity, dt);
        auto position = builder.createLoad(positionAccess, spv::NoPrecision);
        auto updatedPosition = builder.createBinOp(spv::OpFAdd, vec3Type, velocityTimesDeltaTime, position);
        builder.createStore(updatedPosition, positionAccess);

        auto tmpLife = builder.createLoad(lifeAccess, spv::NoPrecision);
        auto updatedLife = builder.createBinOp(spv::OpFSub, float32Type, tmpLife, dt);
        builder.createStore(updatedLife, lifeAccess);

        auto castParticleID = builder.createUnaryOp(spv::OpConvertUToF, float32Type, particleIndex);

        // user-specific code
        Variables vars{};
        vars.shaderMode = shaderMode;
        vars.compute.deltaTime = dt;
        vars.emissionID = emissionID;
        vars.position = positionAccess;
        vars.life = builder.createLoad(lifeAccess, spv::NoPrecision);
        vars.velocity = velocityAccess;
        vars.size = sizeAccess;
        vars.particleIndex = castParticleID;
        for(auto& expr : expressions) {
            builder.getStringId(expr->toString());

            SPIRVisitor exprBuilder(builder, vars);
            exprBuilder.visit(expr);
        }

        builder.leaveFunction();
        auto entryPoint = builder.addEntryPoint(spv::ExecutionModelGLCompute, mainFunction, "main");
        entryPoint->addIdOperand(globalInvocationId);
        entryPoint->addIdOperand(particleStats);
        entryPoint->addIdOperand(descriptorSet);

        builder.addExecutionMode(mainFunction, spv::ExecutionModeLocalSize, 1024, 1, 1);
    }
}