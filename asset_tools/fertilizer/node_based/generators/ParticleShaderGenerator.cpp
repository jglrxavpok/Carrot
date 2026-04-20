//
// Created by jglrxavpok on 19/05/2021.
//

#include "ParticleShaderGenerator.h"

#include <fstream>

#include "core/expressions/ExpressionVisitor.h"
#include <stack>
#include <strstream>
#include <utility>
#include <core/utils/stringmanip.h>
#include <SPIRV/disassemble.h>
#include <SPIRV/SpvBuilder.h>

#include "node_based/nodes/VariableNodes.h"
#include "node_based/nodes/TerminalNodes.h"
#include "SPIRV/GLSL.std.450.h"

namespace Fertilizer {
    using namespace spv;

    struct Variables {
        Id glslImport;

        spv::Builder::AccessChain position;
        spv::Builder::AccessChain life;
        spv::Builder::AccessChain velocity;
        spv::Builder::AccessChain size;
        Id particleIndex;
        Id emissionID;

        union {
            struct {
                Id deltaTime;
            } compute;

            struct {
                Id particleColor;
                Id fragmentPosition;
            } fragment;
        };

        struct {
            Id floatType;
            Id float4Type;
        } types;

        Id particleStats;
        Id texturesArray;
        Id linearSampler;
        Id nearestSampler;

        ParticleShaderMode shaderMode;
    };

    spv::Builder::AccessChain accessChain(spv::Builder& builder, Id resultType, Id base, const std::vector<Id>& offsets) {
        builder.clearAccessChain();
        builder.setAccessChainLValue(base);
        for (const Id& offset : offsets) {
            builder.accessChainPush(offset, {}, 0);
        }
        verify(resultType == builder.getResultingAccessChainType(), "resultType is not valid");
        return builder.getAccessChain();
    }

    spv::Id accessChainLoad(spv::Builder& builder, const spv::Builder::AccessChain& accessChain) {
        builder.setAccessChain(accessChain);
        return builder.accessChainLoad(spv::NoPrecision, spv::NoPrecision, spv::NoPrecision, builder.getResultingAccessChainType());
    }

    void accessChainStore(spv::Builder& builder, const spv::Builder::AccessChain& accessChain, const Id& valueToStore) {
        builder.setAccessChain(accessChain);
        builder.accessChainStore(valueToStore, spv::DecorationNonUniform);
    }

    spv::Builder::AccessChain continueChain(spv::Builder& builder, const spv::Builder::AccessChain& accessChain, const std::vector<Id>& valueToStore) {
        builder.setAccessChain(accessChain);
        for (const Id& o : valueToStore) {
            builder.accessChainPush(o, {}, 0);
        }
        return builder.getAccessChain();
    }

    class SPIRVisitor: public Carrot::ExpressionVisitor {
    public:
        inline explicit SPIRVisitor(spv::Builder& builder, Variables vars): builder(builder), vars(vars) {}

    public:
        void visitConstant(Carrot::ConstantExpression& expression) override {
            auto value = expression.getValue();
            auto exprType = expression.getType();
            types.push(exprType);

            if(exprType == Carrot::ExpressionTypes::Float) {
                ids.push(builder.makeFloatConstant(value.asFloat));
            } else if(exprType == Carrot::ExpressionTypes::Int) {
                ids.push(builder.makeIntConstant(value.asInt));
            } else if(exprType == Carrot::ExpressionTypes::Bool) {
                ids.push(builder.makeBoolConstant(value.asBool));
            } else {
                throw std::runtime_error("Invalid type");
            }
        }

        void visitGetVariable(Carrot::GetVariableExpression& expression) override {
            const auto& var = expression.getVariableName();

            if(var == VariableNode::getInternalName(VariableNodeType::GetSize)) {
                ids.push(accessChainLoad(builder, vars.size));
                types.push(Carrot::ExpressionTypes::Float);
            } else if(var == VariableNode::getInternalName(VariableNodeType::GetVelocity)) {
                auto access = accessChainLoad(builder, continueChain(builder, vars.velocity, {builder.makeIntConstant(expression.getSubIndex())}));
                ids.push(access);
                types.push(Carrot::ExpressionTypes::Float);
            } else if(var == VariableNode::getInternalName(VariableNodeType::GetLife)) {
                ids.push(accessChainLoad(builder, vars.life));
                types.push(Carrot::ExpressionTypes::Float);
            } else if(var == VariableNode::getInternalName(VariableNodeType::GetParticleIndex)) {
                ids.push(vars.particleIndex);
                types.push(Carrot::ExpressionTypes::Float);
            } else if(var == VariableNode::getInternalName(VariableNodeType::GetEmissionID)) {
                ids.push(vars.emissionID);
                types.push(Carrot::ExpressionTypes::Float);
            } else if(var == VariableNode::getInternalName(VariableNodeType::GetPosition)) {
                auto access = accessChainLoad(builder, continueChain(builder, vars.position,
                                                        {builder.makeIntConstant(expression.getSubIndex())}));
                ids.push(access);
                types.push(Carrot::ExpressionTypes::Float);
            }
        // === COMPUTE ===
            else if(var == VariableNode::getInternalName(VariableNodeType::GetDeltaTime)) {
                assert(vars.shaderMode == ParticleShaderMode::Compute);
                ids.push(vars.compute.deltaTime);
                types.push(Carrot::ExpressionTypes::Float);
            }
        // === FRAGMENT ===
            else if(var == VariableNode::getInternalName(VariableNodeType::GetFragmentPosition)) {
                assert(vars.shaderMode == ParticleShaderMode::Fragment);
                auto access = accessChainLoad(builder, accessChain(builder, vars.types.floatType, vars.fragment.fragmentPosition, {builder.makeIntConstant(expression.getSubIndex())}));
                ids.push(access);
                types.push(Carrot::ExpressionTypes::Float);
            }
        // ================
            else {
                throw std::runtime_error("Unsupported variable type: "+var);
            }
        }

        void visitSetVariable(Carrot::SetVariableExpression& expression) override {
            visit(expression.getValue());

            const auto& var = expression.getVariableName();

            auto typeToStore = popType();

        // === COMPUTE ===
            if(var == TerminalNode::getInternalName(TerminalNodeType::SetSize)) {
                throwInvalidType(Carrot::ExpressionTypes::Float, typeToStore, "set size");
                accessChainStore(builder, vars.size, popID());
            } else if(var == TerminalNode::getInternalName(TerminalNodeType::SetVelocity)) {
                throwInvalidType(Carrot::ExpressionTypes::Float, typeToStore, "set velocity component");
                auto access = continueChain(builder, vars.velocity,
                                                        {builder.makeIntConstant(expression.getSubIndex())});
                accessChainStore(builder, access, popID());
            }
        // === FRAGMENT ===
            else if(var == TerminalNode::getInternalName(TerminalNodeType::SetOutputColor)) {
                throwInvalidType(Carrot::ExpressionTypes::Float, typeToStore, "set output color component");
                assert(vars.shaderMode == ParticleShaderMode::Fragment);
                auto access = accessChain(builder, vars.types.floatType, vars.fragment.particleColor,
                                                        {builder.makeIntConstant(expression.getSubIndex())});
                accessChainStore(builder, access, popID());
            }
            else if(var == TerminalNode::getInternalName(TerminalNodeType::DiscardPixel)) {
                throwInvalidType(Carrot::ExpressionTypes::Bool, typeToStore, "discard pixel");
                assert(vars.shaderMode == ParticleShaderMode::Fragment);
                auto shouldDiscard = popID();

                Block& thenBlock = builder.makeNewBlock();
                Block& mergeBlock = builder.makeNewBlock();
                { // selection merge
                    Instruction* merge = new Instruction(OpSelectionMerge);
                    merge->addIdOperand(mergeBlock.getId());
                    merge->addImmediateOperand(spv::SelectionControlMaskNone);
                    builder.getBuildPoint()->addInstruction(std::unique_ptr<Instruction>(merge));
                }

                builder.createConditionalBranch(shouldDiscard, &thenBlock, &mergeBlock);

                builder.setBuildPoint(&thenBlock);
                builder.createNoResultOp(spv::OpKill);

                builder.setBuildPoint(&mergeBlock);
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

        void visitMin(Carrot::MinExpression& expression) override {
            visit(expression.getOperand1());
            visit(expression.getOperand2());
            visitGLSLBiFunction(GLSLstd450FMin);
        }

        void visitMax(Carrot::MaxExpression& expression) override {
            visit(expression.getOperand1());
            visit(expression.getOperand2());
            visitGLSLBiFunction(GLSLstd450FMax);
        }

        void visitLess(Carrot::LessExpression& expression) override {
            visitCompareOp(spv::OpFOrdLessThan, expression.getOperand1(), expression.getOperand2());
        }

        void visitLessOrEquals(Carrot::LessOrEqualsExpression& expression) override {
            visitCompareOp(spv::OpFOrdLessThanEqual, expression.getOperand1(), expression.getOperand2());
        }

        void visitGreater(Carrot::GreaterExpression& expression) override {
            visitCompareOp(spv::OpFOrdGreaterThan, expression.getOperand1(), expression.getOperand2());
        }

        void visitGreaterOrEquals(Carrot::GreaterOrEqualsExpression& expression) override {
            visitCompareOp(spv::OpFOrdGreaterThanEqual, expression.getOperand1(), expression.getOperand2());
        }

        void visitEquals(Carrot::EqualsExpression& expression) override {
            visitCompareOp(spv::OpFOrdEqual, expression.getOperand1(), expression.getOperand2());
        }

        void visitNotEquals(Carrot::NotEqualsExpression& expression) override {
            visitCompareOp(spv::OpFOrdNotEqual, expression.getOperand1(), expression.getOperand2());
        }

        void visitOr(Carrot::OrExpression& expression) override {
            visitBinOp(spv::OpLogicalOr, expression.getOperand1(), expression.getOperand2());
        }

        void visitAnd(Carrot::AndExpression& expression) override {
            visitBinOp(spv::OpLogicalAnd, expression.getOperand1(), expression.getOperand2());
        }

        void visitXor(Carrot::XorExpression& expression) override {
            // SPIR-V has no logical XOR operation, so we have to implement it ourselves

            visitBinOp(spv::OpAtomicOr, expression.getOperand1(), expression.getOperand2());
            auto orResult = popID();
            auto orResultType = popType();
            assert(orResultType == Carrot::ExpressionTypes::Bool);

            visitBinOp(spv::OpAtomicAnd, expression.getOperand1(), expression.getOperand2());
            auto andResult = popID();
            auto andResultType = popType();
            assert(andResultType == Carrot::ExpressionTypes::Bool);

            auto negatedAnd = builder.createUnaryOp(spv::OpLogicalNot, builder.makeBoolType(), andResult);
            auto xorResult = builder.createBinOp(spv::OpLogicalAnd, builder.makeBoolType(), negatedAnd, orResult);
            ids.push(xorResult);
            types.push(Carrot::ExpressionTypes::Bool);
        }

        void visitBoolNegate(Carrot::BoolNegateExpression& expression) override {
            auto prevResult = popID();
            auto prevResultType = popType();
            assert(prevResultType == Carrot::ExpressionTypes::Bool);
            auto negated = builder.createUnaryOp(spv::OpLogicalNot, builder.makeBoolType(), prevResult);
            ids.push(negated);
            types.push(Carrot::ExpressionTypes::Bool);
        }

        void visitSin(Carrot::SinExpression& expression) override {
            visit(expression.getOperand());
            visitGLSLFunction(GLSLstd450Sin);
        }

        void visitCos(Carrot::CosExpression& expression) override {
            visit(expression.getOperand());
            visitGLSLFunction(GLSLstd450Cos);
        }

        void visitTan(Carrot::TanExpression& expression) override {
            visit(expression.getOperand());
            visitGLSLFunction(GLSLstd450Tan);
        }

        void visitExp(Carrot::ExpExpression& expression) override {
            visit(expression.getOperand());
            visitGLSLFunction(GLSLstd450Exp);
        }

        void visitAbs(Carrot::AbsExpression& expression) override {
            visit(expression.getOperand());
            visitGLSLFunction(GLSLstd450FAbs);
        }

        void visitSqrt(Carrot::SqrtExpression& expression) override {
            visit(expression.getOperand());
            visitGLSLFunction(GLSLstd450Sqrt);
        }

        void visitLog(Carrot::LogExpression& expression) override {
            visit(expression.getOperand());
            visitGLSLFunction(GLSLstd450Log);
        }

        void visitPlaceholder(Carrot::PlaceholderExpression& expression) override {
            throw std::runtime_error("Cannot accept placeholder expression!");
        }

        void visitPrefixed(Carrot::PrefixedExpression& expression) override {
            visit(expression.getPrefix());
            visit(expression.getExpression());
        }

        void visitOnce(Carrot::OnceExpression& expression) override {
            if(alreadyVisited.contains(expression.getUUID())) {
                return;
            }
            alreadyVisited.insert(expression.getUUID());
            visit(expression.getExpressionToExecute());
        }

        void visitSampleImage(Carrot::SampleImageExpression& expression) override {
            // 1. generate slot for image
            auto [iter, isNew] = imageIndices.try_emplace(expression.getImagePath());
            if (isNew) {
                Id imageType = builder.makeImageType(builder.makeFloatType(32), spv::Dim2D, false, false, false, 1, spv::ImageFormatUnknown);
                Id sampledImageType = builder.makeSampledImageType(imageType);

                // TODO: allow selection of sampler
                const i32 imageIndex = static_cast<i32>(imageIndices.size()-1);
                spv::Builder::AccessChain textureAccess = accessChain(builder, imageType, vars.texturesArray, {builder.makeIntConstant(imageIndex)});
                Id image = accessChainLoad(builder, textureAccess);

                Id sampler = builder.createLoad(vars.linearSampler, spv::NoPrecision);
                Id sampledImage = builder.createBinOp(spv::Op::OpSampledImage, sampledImageType, image, sampler);

                iter->second = sampledImage;
            }

            const Id imageId = iter->second;

            // 2. generate UV vector
            visit(expression.getU());
            visit(expression.getV());
            auto vID = popID();
            throwInvalidType(Carrot::ExpressionTypes::Float, popType(), "sample image V");
            auto uID = popID();
            throwInvalidType(Carrot::ExpressionTypes::Float, popType(), "sample image U");

            auto vectorID = builder.createCompositeConstruct(builder.makeVectorType(vars.types.floatType, 2), {uID, vID});

            // 3. sample image
            auto resultID = builder.createOp(spv::OpImageSampleImplicitLod, vars.types.float4Type, {imageId, vectorID});
            ids.push(resultID);
            types.push(Carrot::ExpressionTypes::Color);
        }

        void visitGetVectorComponent(Carrot::GetVectorComponentExpression& expression) override {
            visit(expression.getVectorInput());
            auto vectorID = popID();
            auto vectorType = popType();
            throwInvalidType(Carrot::ExpressionTypes::Color, vectorType, "make vector");
            auto componentID = builder.createCompositeExtract(vectorID, builder.makeFloatType(32), expression.getComponentIndex());
            ids.push(componentID);
            types.push(Carrot::ExpressionTypes::Float);
        }

        void visitMakeVector(Carrot::MakeVectorExpression& expression) override {
            std::vector<Id> componentIDs;
            for (const auto& pComponentExpr : expression.getInputs()) {
                visit(pComponentExpr);
                componentIDs.emplace_back(popID());
                auto componentType = popType();
                throwInvalidType(Carrot::ExpressionTypes::Float, componentType, "make vector");
            }
            Id float32Type = builder.makeFloatType(32);
            Id typeID = builder.makeVectorType(float32Type, expression.getVectorSize());
            auto vectorID = builder.createCompositeConstruct(typeID, componentIDs);
            ids.push(vectorID);
            types.push(Carrot::ExpressionTypes::Color);
        }

    private:
        spv::Builder& builder;
        std::stack<Id> ids;
        std::stack<Carrot::ExpressionType> types;
        Variables vars;
        std::unordered_set<Carrot::UUID> alreadyVisited;
        std::unordered_map<std::string, Id> imageIndices;

        void visitGLSLFunction(GLSLstd450 function) {
            auto operand = popID();
            auto operandType = popType();
            assert(operandType == Carrot::ExpressionTypes::Float);
            auto result = builder.createTriOp(spv::OpExtInst, builder.makeFloatType(32), vars.glslImport, function, operand);
            ids.push(result);
            types.push(Carrot::ExpressionTypes::Float);
        }

        void visitGLSLBiFunction(GLSLstd450 function) {
            auto operand1 = popID();
            auto operandType1 = popType();
            assert(operandType1 == Carrot::ExpressionTypes::Float);
            auto operand2 = popID();
            auto operandType2 = popType();
            assert(operandType2 == Carrot::ExpressionTypes::Float);

            auto result = builder.createOp(spv::OpExtInst, builder.makeFloatType(32), std::vector<spv::Id>{ vars.glslImport, static_cast<spv::Id>(function), operand1, operand2 });
            ids.push(result);
            types.push(Carrot::ExpressionTypes::Float);
        }

        void visitBinOp(spv::Op op, std::shared_ptr<Carrot::Expression> left, std::shared_ptr<Carrot::Expression> right) {
            visit(left);
            visit(right);
            auto rightID = popID();
            auto leftID = popID();

            auto leftType = popType();
            auto rightType = popType();
            if(leftType != rightType) {
                throw std::runtime_error("Cannot operate on operands of different types");
            }

            const auto& returnType = leftType;

            ids.push(builder.createBinOp(op, asSpirType(returnType), leftID, rightID));
            types.push(returnType);
        }

        void visitCompareOp(spv::Op op, std::shared_ptr<Carrot::Expression> left, std::shared_ptr<Carrot::Expression> right) {
            visit(left);
            visit(right);
            auto rightID = popID();
            auto leftID = popID();

            auto leftType = popType();
            auto rightType = popType();
            if(leftType != rightType) {
                throw std::runtime_error("Cannot operate on operands of different types");
            }

            auto returnType = Carrot::ExpressionTypes::Bool;

            ids.push(builder.createBinOp(op, asSpirType(returnType), leftID, rightID));
            types.push(returnType);
        }

        static void throwInvalidType(const Carrot::ExpressionType& expected, const Carrot::ExpressionType& actual, const std::string& where) {
            if(expected != actual) {
                throw std::runtime_error("Invalid type, expected " + expected.name() + ", got " + actual.name() + ", at " + where);
            }
        }

        [[nodiscard]] Id asSpirType(const Carrot::ExpressionType& type) const {
            if(type == Carrot::ExpressionTypes::Void) {
                return builder.makeVoidType();
            }
            if(type == Carrot::ExpressionTypes::Float) {
                return builder.makeFloatType(32);
            }
            if(type == Carrot::ExpressionTypes::Int) {
                return builder.makeIntConstant(32);
            }
            if(type == Carrot::ExpressionTypes::Bool) {
                return builder.makeBoolType();
            }
            throw std::runtime_error("Unsupported type");
        }

        Id popID() {
            auto id = ids.top();
            ids.pop();
            return id;
        }

        Carrot::ExpressionType popType() {
            auto type = types.top();
            types.pop();
            return type;
        }
    };

    ParticleShaderGenerator::ParticleShaderGenerator(ParticleShaderMode shaderMode, const std::string& projectName): shaderMode(shaderMode), projectName(projectName) {

    }

    std::vector<uint32_t> ParticleShaderGenerator::compileToSPIRV(const std::vector<std::shared_ptr<Carrot::Expression>>& expressions) {
        SpvBuildLogger logger;
        Builder builder(spv::SpvVersion::Spv_1_5, 0, &logger);

        builder.setSource(spv::SourceLanguageGLSL, 450);
        builder.setDebugMainSourceFile(projectName);
        builder.addCapability(spv::CapabilityShader);
        builder.addCapability(spv::CapabilityShaderNonUniform);
        auto glslImport = builder.import("GLSL.std.450");
        builder.setMemoryModel(spv::AddressingModelLogical, spv::MemoryModelGLSL450);

        auto uint32Type = builder.makeUintType(32);
        auto float32Type = builder.makeFloatType(32);
        auto vec3Type = builder.makeVectorType(float32Type, 3);
        /*

        struct Particle {
            vec3 position;
            float life;
            vec3 velocity;
            float size;

            uint id;
            uint emitterID
        };

        struct Emitter {
            mat4 emitterTransform;
        }

        layout(set = 1, binding = 0, scalar) buffer Particles {
            Particle particles[];
        };

        // TODO: unavailable for pixel & compute stages for now (only vertex)
        layout(set = 1, binding = 1, scalar) uniform Emitters {
            Emitter emitter[];
        };

        layout(set = 1, binding = 2, scalar) buffer ParticleStats;

        layout(set = 1, binding = 3) image2D textures[];
        layout(set = 1, binding = 4) sampler2D linearSampler;
        layout(set = 1, binding = 5) sampler2D pointSampler;
         */
        auto particleType = builder.makeStructType(std::vector<Id>{
                vec3Type,
                float32Type,
                vec3Type,
                float32Type,

                uint32Type,
                uint32Type
        }, "Particle");
        builder.addMemberName(particleType, 0, "position");
        builder.addMemberName(particleType, 1, "life");
        builder.addMemberName(particleType, 2, "velocity");
        builder.addMemberName(particleType, 3, "size");
        builder.addMemberName(particleType, 4, "id");
        builder.addMemberName(particleType, 5, "emitterID");

        builder.addMemberDecoration(particleType, 0, spv::DecorationOffset, 0);
        builder.addMemberDecoration(particleType, 1, spv::DecorationOffset, 12);
        builder.addMemberDecoration(particleType, 2, spv::DecorationOffset, 16);
        builder.addMemberDecoration(particleType, 3, spv::DecorationOffset, 28);
        builder.addMemberDecoration(particleType, 4, spv::DecorationOffset, 32);
        builder.addMemberDecoration(particleType, 5, spv::DecorationOffset, 36);

        auto maxParticleCount = builder.makeIntConstant(1000, true);
        builder.addName(maxParticleCount, "MAX_PARTICLE_COUNT");
        builder.addDecoration(maxParticleCount, spv::DecorationSpecId, 0);

        auto particleArrayType = builder.makeArrayType(particleType, maxParticleCount, 0);
        builder.addDecoration(particleArrayType, spv::DecorationArrayStride, 40);
        auto storageBufferParticles = builder.makeStructType(std::vector<Id>{particleArrayType}, "Particles");
        builder.addMemberName(storageBufferParticles, 0, "particles");
        builder.addDecoration(storageBufferParticles, spv::DecorationBlock);
        builder.addMemberDecoration(storageBufferParticles, 0, spv::DecorationOffset, 0);

        auto descriptorSet = builder.createVariable(spv::NoPrecision, spv::StorageClassStorageBuffer, storageBufferParticles);
        builder.addName(descriptorSet, "particleData");
        builder.addDecoration(descriptorSet, spv::DecorationDescriptorSet, 1);
        builder.addDecoration(descriptorSet, spv::DecorationBinding, 0);

        // Carrot::ParticleStatistics
        auto particleStatsType = builder.makeStructType(std::vector<Id>{builder.makeFloatType(32), builder.makeUintType(32)}, "ParticleStatistics");
        builder.addMemberName(particleStatsType, 0, "deltaTime");
        builder.addMemberName(particleStatsType, 1, "particleCount");

        builder.addDecoration(particleStatsType, spv::DecorationBlock);
        builder.addMemberDecoration(particleStatsType, 0, spv::DecorationOffset, 0);
        builder.addMemberDecoration(particleStatsType, 1, spv::DecorationOffset, 4);

        particleStats = builder.createVariable(spv::NoPrecision, spv::StorageClassStorageBuffer, particleStatsType);
        builder.addDecoration(particleStats, spv::DecorationDescriptorSet, 1);
        builder.addDecoration(particleStats, spv::DecorationBinding, 2);
        builder.addName(particleStats, "particleStats");

        // textures array
        auto imgType = builder.makeImageType(float32Type, spv::Dim2D, false, false, false, 1, spv::ImageFormatUnknown);
        auto texturesArrayType = builder.makeRuntimeArray(imgType);
        texturesArray = builder.createVariable(spv::NoPrecision, spv::StorageClassUniformConstant, texturesArrayType);
        builder.addDecoration(texturesArray, spv::DecorationDescriptorSet, 1);
        builder.addDecoration(texturesArray, spv::DecorationBinding, 3);
        builder.addName(texturesArray, "textures");

        // samplers
        auto samplerType = builder.makeSamplerType();
        linearSampler = builder.createVariable(spv::NoPrecision, spv::StorageClassUniformConstant, samplerType);
        builder.addDecoration(linearSampler, spv::DecorationDescriptorSet, 1);
        builder.addDecoration(linearSampler, spv::DecorationBinding, 4);
        builder.addName(linearSampler, "linearSampler");

        nearestSampler = builder.createVariable(spv::NoPrecision, spv::StorageClassUniformConstant, samplerType);
        builder.addDecoration(nearestSampler, spv::DecorationDescriptorSet, 1);
        builder.addDecoration(nearestSampler, spv::DecorationBinding, 5);
        builder.addName(nearestSampler, "nearestSampler");

        switch(shaderMode) {
            case ParticleShaderMode::Compute:
                generateCompute(builder, glslImport, descriptorSet, expressions);
                break;

            case ParticleShaderMode::Fragment:
                generateFragment(builder, glslImport, descriptorSet, expressions);
                break;
        }

        std::vector<uint32_t> output;
        builder.dump(output);



        std::cout << "SpvBuilder output:\n";
        std::cerr << logger.getAllMessages();
        std::cout << "\nDisassembly:\n";

        std::stringstream sstream{};
        spv::Disassemble(sstream, output);
        std::cout << sstream.str();

        std::cout << "\n=== spir-v cross: ===\n";

        {
            std::ofstream out {"spvbuilder.bin", std::ios::binary};
            out.write(reinterpret_cast<const std::ostream::char_type*>(output.data()), output.size() * sizeof(std::uint32_t));
        }

        system("spirv-cross -V spvbuilder.bin");

        std::cout << "\n=== spir-val: ===\n";
        system("spirv-val spvbuilder.bin");

        std::cout << "\n=== END of SpvBuilder output ===\n";

        return std::move(output);
    }

    void ParticleShaderGenerator::generateFragment(spv::Builder& builder, spv::Id glslImport, spv::Id descriptorSet, const std::vector<std::shared_ptr<Carrot::Expression>>& expressions) {
        spv::Block* functionBlock;
        Function* mainFunction = builder.makeFunctionEntry(spv::NoPrecision, builder.makeVoidType(), "main", {}, {}, {}, &functionBlock);

        auto uint32Type = builder.makeUintType(32);
        auto float32Type = builder.makeFloatType(32);
        auto vec2Type = builder.makeVectorType(float32Type, 2);
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

        auto inFragPosition = builder.createVariable(spv::NoPrecision, spv::StorageClassInput, vec2Type);
        builder.addName(inFragPosition, "inFragPosition");
        builder.addDecoration(inFragPosition, spv::DecorationLocation, 1);

        auto varParticleIndex = builder.createVariable(spv::NoPrecision, spv::StorageClassFunction, uint32Type);
        auto tmpParticleIndex = builder.createLoad(inParticleIndex, spv::NoPrecision);
        builder.createStore(tmpParticleIndex, varParticleIndex);
        builder.addName(varParticleIndex, "particleIndex");

        auto varFragmentPosition = builder.createVariable(spv::NoPrecision, spv::StorageClassFunction, vec2Type);
        auto tmpFragmentPosition = builder.createLoad(inFragPosition, spv::NoPrecision);
        builder.createStore(tmpFragmentPosition, varFragmentPosition);
        builder.addName(varFragmentPosition, "fragmentPosition");

        auto particleIndex = builder.createLoad(varParticleIndex, spv::NoPrecision);

        auto velocityAccess = accessChain(builder, vec3Type, descriptorSet, {builder.makeIntConstant(0), particleIndex, builder.makeIntConstant(2)});
        auto positionAccess = accessChain(builder, vec3Type, descriptorSet, {builder.makeIntConstant(0), particleIndex, builder.makeIntConstant(0)});
        auto lifeAccess = accessChain(builder, float32Type, descriptorSet, {builder.makeIntConstant(0), particleIndex, builder.makeIntConstant(1)});
        auto sizeAccess = accessChain(builder, float32Type, descriptorSet, {builder.makeIntConstant(0), particleIndex, builder.makeIntConstant(3)});

        auto emissionIDAccess = accessChain(builder, uint32Type, descriptorSet, {builder.makeIntConstant(0), particleIndex, builder.makeIntConstant(4)});

        auto castParticleID = builder.createUnaryOp(spv::OpConvertUToF, float32Type, particleIndex);

        auto varFinalColor = builder.createVariable(spv::NoPrecision, spv::StorageClassFunction, vec4Type);
        builder.addName(varFinalColor, "finalColor");
        auto float1 = builder.makeFloatConstant(1.0f);
        builder.createStore(builder.createCompositeConstruct(vec4Type, {float1, float1, float1, float1}), varFinalColor);

        // user-specific code
        Variables vars{};
        vars.glslImport = glslImport;
        vars.shaderMode = shaderMode;
        vars.fragment.particleColor = varFinalColor;
        vars.fragment.fragmentPosition = varFragmentPosition;
        vars.emissionID = builder.createUnaryOp(spv::OpConvertUToF, float32Type, accessChainLoad(builder, emissionIDAccess));
        vars.position = positionAccess;
        vars.life = lifeAccess;
        vars.velocity = velocityAccess;
        vars.size = sizeAccess;
        vars.particleIndex = castParticleID;
        vars.types.floatType = float32Type;
        vars.types.float4Type = vec4Type;
        vars.texturesArray = texturesArray;
        vars.linearSampler = linearSampler;
        vars.nearestSampler = nearestSampler;
        vars.particleStats = particleStats;
        for(auto& expr : expressions) {
            builder.getStringId(expr->toString());

            builder.setDebugSourceLocation(0, expr->toString().c_str());

            SPIRVisitor exprBuilder(builder, vars);
            exprBuilder.visit(expr);
        }

        builder.createStore(builder.createLoad(varFinalColor, spv::NoPrecision), outColor);
        builder.createStore(builder.makeUintConstant(0), outIntProperties);

        builder.leaveFunction();
        auto entryPoint = builder.addEntryPoint(spv::ExecutionModelFragment, mainFunction, "main");
        entryPoint->addIdOperand(inParticleIndex);
        entryPoint->addIdOperand(inFragPosition);
        entryPoint->addIdOperand(outColor);
        entryPoint->addIdOperand(outViewPosition);
        entryPoint->addIdOperand(outNormal);
        entryPoint->addIdOperand(outIntProperties);
        entryPoint->addIdOperand(descriptorSet);
        entryPoint->addIdOperand(vars.texturesArray);
        entryPoint->addIdOperand(vars.linearSampler);
        entryPoint->addIdOperand(vars.nearestSampler);
        entryPoint->addIdOperand(vars.particleStats);

        builder.addExecutionMode(mainFunction, spv::ExecutionModeOriginUpperLeft);
    }

    void ParticleShaderGenerator::generateCompute(spv::Builder& builder, spv::Id glslImport, spv::Id descriptorSet, const std::vector<std::shared_ptr<Carrot::Expression>>& expressions) {
        spv::Block* functionBlock;
        Function* mainFunction = builder.makeFunctionEntry(spv::NoPrecision, builder.makeVoidType(), "main", {}, {}, {}, &functionBlock);

        auto int32Type = builder.makeIntType(32);
        auto uint32Type = builder.makeUintType(32);
        auto float32Type = builder.makeFloatType(32);
        auto vec3Type = builder.makeVectorType(float32Type, 3);
        auto vec4Type = builder.makeVectorType(float32Type, 4);

        auto globalInvocationId = builder.createVariable(spv::NoPrecision,
                                                         spv::StorageClassInput,
                                                         builder.makeVectorType(builder.makeUintType(32), 3),
                                                         "gl_GlobalInvocationID");
        builder.addDecoration(globalInvocationId, spv::DecorationBuiltIn, spv::BuiltInGlobalInvocationId);


        auto gl_WorkGroupSize = builder.makeCompositeConstant(builder.makeVectorType(builder.makeUintType(32), 3),
                                                              std::vector{
                                                                      builder.makeUintConstant(1024),
                                                                      builder.makeUintConstant(1),
                                                                      builder.makeUintConstant(1)
                                                              });
        builder.addDecoration(gl_WorkGroupSize, spv::DecorationBuiltIn, spv::BuiltIn::BuiltInWorkgroupSize);

        auto dtAccess = accessChain(builder, float32Type, particleStats, std::vector<Id>{builder.makeIntConstant(0)});
        auto dt = accessChainLoad(builder, dtAccess);
        builder.addName(dt, "dt");

        auto particleIndexLoaded = accessChainLoad(builder, accessChain(builder, uint32Type, globalInvocationId, std::vector<Id>{builder.makeIntConstant(0)}));
        auto particleIndexVar = builder.createVariable(spv::NoPrecision, StorageClass::StorageClassFunction, int32Type, "particleIndex");
        builder.createStore(builder.createOp(spv::Op::OpBitcast, int32Type, std::vector<Id>{particleIndexLoaded}), particleIndexVar);

        auto particleIndex = builder.createLoad(particleIndexVar, spv::NoPrecision);

        auto emissionIDAccess = accessChain(builder, uint32Type, descriptorSet, {builder.makeIntConstant(0), particleIndex, builder.makeIntConstant(4)});
        auto emissionID = accessChainLoad(builder, emissionIDAccess);
        builder.addName(emissionID, "emissionID");

        auto velocityAccess = accessChain(builder, vec3Type, descriptorSet, {builder.makeIntConstant(0), particleIndex, builder.makeIntConstant(2)});
        auto velocity = accessChainLoad(builder, velocityAccess);
        builder.addName(velocity, "velocity");

        auto positionAccess = accessChain(builder, vec3Type, descriptorSet, {builder.makeIntConstant(0), particleIndex, builder.makeIntConstant(0)});
        auto position = accessChainLoad(builder, positionAccess);

        auto lifeAccess = accessChain(builder, float32Type, descriptorSet, {builder.makeIntConstant(0), particleIndex, builder.makeIntConstant(1)});
        auto life = accessChainLoad(builder, lifeAccess);
        builder.addName(life, "life");

        auto sizeAccess = accessChain(builder, float32Type, descriptorSet, {builder.makeIntConstant(0), particleIndex, builder.makeIntConstant(3)});

        auto particleCountAccess = accessChain(builder, uint32Type, particleStats, std::vector<Id>{builder.makeIntConstant(1)});


        // Generate: if(particleIndex >= totalCount)
        {
            auto particleCount = accessChainLoad(builder, particleCountAccess);
            auto shouldContinue = builder.createBinOp(spv::OpUGreaterThanEqual, builder.makeBoolType(), particleIndex, particleCount);
            spv::Builder::If ifValidIndex(shouldContinue, spv::SelectionControlMaskNone, builder);
            builder.makeReturn(false);
            ifValidIndex.makeEndIf();
        }

        // update position
        auto velocityTimesDeltaTime = builder.createBinOp(spv::OpVectorTimesScalar, vec3Type, velocity, dt);
        auto updatedPosition = builder.createBinOp(spv::OpFAdd, vec3Type, velocityTimesDeltaTime, position);
        accessChainStore(builder, positionAccess, updatedPosition);

        auto updatedLife = builder.createBinOp(spv::OpFSub, float32Type, life, dt);
        accessChainStore(builder, lifeAccess, updatedLife);

        auto castParticleID = builder.createUnaryOp(spv::OpConvertUToF, float32Type, particleIndex);

        // user-specific code
        Variables vars{};
        vars.glslImport = glslImport;
        vars.shaderMode = shaderMode;
        vars.compute.deltaTime = dt;
        vars.emissionID = builder.createUnaryOp(spv::OpConvertUToF, float32Type, emissionID);
        vars.position = positionAccess;
        vars.life = lifeAccess;
        vars.velocity = velocityAccess;
        vars.size = sizeAccess;
        vars.particleIndex = castParticleID;
        vars.types.floatType = float32Type;
        vars.types.float4Type = vec4Type;
        vars.texturesArray = texturesArray;
        vars.linearSampler = linearSampler;
        vars.nearestSampler = nearestSampler;
        vars.particleStats = particleStats;
        for(auto& expr : expressions) {
            builder.getStringId(expr->toString());

            SPIRVisitor exprBuilder(builder, vars);
            exprBuilder.visit(expr);
        }

        builder.leaveFunction();
        auto entryPoint = builder.addEntryPoint(spv::ExecutionModelGLCompute, mainFunction, "main");
        entryPoint->addIdOperand(globalInvocationId);
        entryPoint->addIdOperand(descriptorSet);
        entryPoint->addIdOperand(vars.texturesArray);
        entryPoint->addIdOperand(vars.linearSampler);
        entryPoint->addIdOperand(vars.nearestSampler);
        entryPoint->addIdOperand(vars.particleStats);

        builder.addExecutionMode(mainFunction, spv::ExecutionModeLocalSize, 1024, 1, 1);
    }
}