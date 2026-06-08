//
// Created by jglrxavpok on 19/05/2021.
//

#include "ParticleShaderGenerator.h"

#include <FileIncluder.h>
#include <fstream>
#include <SlangCompiler.h>

#include "core/expressions/ExpressionVisitor.h"
#include <stack>
#include <strstream>
#include <utility>
#include <core/io/FileSystemOS.h>
#include <core/io/IO.h>
#include <core/utils/stringmanip.h>
#include <SPIRV/disassemble.h>
#include <SPIRV/SpvBuilder.h>

#include "node_based/nodes/VariableNodes.h"
#include "node_based/nodes/TerminalNodes.h"

namespace Fertilizer {
    using namespace spv;

    class SlangVisitor: public Carrot::IExpressionVisitor<std::string> {
    public:
        SlangVisitor(ParticleShaderGenerator& generator): generator(generator) {}

        inline std::string visitConstant(Carrot::ConstantExpression& expression) override {
            if (expression.getType() == Carrot::ExpressionTypes::Bool) {
                return expression.getValue().asBool ? "true" : "false";
            } else if (expression.getType() == Carrot::ExpressionTypes::Float) {
                return Carrot::sprintf("%f", expression.getValue().asFloat);
            } else if (expression.getType() == Carrot::ExpressionTypes::Int) {
                return Carrot::sprintf("%d", expression.getValue().asInt);
            } else {
                throw std::runtime_error(Carrot::sprintf("Type not handled: %s", expression.getType().name().c_str()));
            }
        }

        inline std::string visitGetVariable(Carrot::GetVariableExpression& expression) override {
            if (expression.getVariableName() == VariableNode::getInternalName(VariableNodeType::GetDeltaTime)) {
                return "dt";
            }
            if (expression.getVariableName() == VariableNode::getInternalName(VariableNodeType::GetEmissionID)) {
                return "particle.emitterID";
            }
            if (expression.getVariableName() == VariableNode::getInternalName(VariableNodeType::GetLife)) {
                return "particle.life";
            }
            if (expression.getVariableName() == VariableNode::getInternalName(VariableNodeType::GetSize)) {
                return "particle.size";
            }
            if (expression.getVariableName() == VariableNode::getInternalName(VariableNodeType::GetOffsetFromCenter)) {
                return "renderInfo.offsetFromCenter[" + std::to_string(expression.getSubIndex()) + "]";
            }
            if (expression.getVariableName() == VariableNode::getInternalName(VariableNodeType::GetUV)) {
                return "renderInfo.uv[" + std::to_string(expression.getSubIndex()) + "]";
            }
            if (expression.getVariableName() == VariableNode::getInternalName(VariableNodeType::GetParticleIndex)) {
                return "index";
            }
            if (expression.getVariableName() == VariableNode::getInternalName(VariableNodeType::GetPosition)) {
                return "particle.position[" + std::to_string(expression.getSubIndex()) + "]";
            }
            if (expression.getVariableName() == VariableNode::getInternalName(VariableNodeType::GetVelocity)) {
                return "particle.velocity[" + std::to_string(expression.getSubIndex()) + "]";
            }
            return expression.getVariableName();
        }

        inline std::string visitSetVariable(Carrot::SetVariableExpression& expression) override {
            if (expression.getVariableName() == TerminalNode::getInternalName(TerminalNodeType::DiscardPixel)) {
                return "out.discardPixel = " + visit(expression.getValue()) + ";";
            }
            if (expression.getVariableName() == TerminalNode::getInternalName(TerminalNodeType::SetSize)) {
                return "out.newSize = " + visit(expression.getValue()) + ";";
            }
            if (expression.getVariableName() == TerminalNode::getInternalName(TerminalNodeType::SetVelocity)) {
                return "out.newVelocity[" + std::to_string(expression.getSubIndex()) + "] = " + visit(expression.getValue()) + ";";
            }
            if (expression.getVariableName() == TerminalNode::getInternalName(TerminalNodeType::SetOutputColor)) {
                return "out.albedo[" + std::to_string(expression.getSubIndex()) + "] = " + visit(expression.getValue()) + ";";
            }
            Carrot::GetVariableExpression equivalentGet{expression.getType(), expression.getVariableName(), expression.getSubIndex()};
            return visitGetVariable(equivalentGet) + " = " + visit(expression.getValue()) + ";";
        }

        inline std::string visitAdd(Carrot::AddExpression& expression) override {
            return "(" + visit(expression.getOperand1()) + ") + (" + visit(expression.getOperand2()) + ")";
        }

        inline std::string visitSub(Carrot::SubExpression& expression) override {
            return "(" + visit(expression.getOperand1()) + ") - (" + visit(expression.getOperand2()) + ")";
        }
        inline std::string visitMult(Carrot::MultExpression& expression) override {
            return "(" + visit(expression.getOperand1()) + ") * (" + visit(expression.getOperand2()) + ")";
        }
        inline std::string visitDiv(Carrot::DivExpression& expression) override {
            return "(" + visit(expression.getOperand1()) + ") / (" + visit(expression.getOperand2()) + ")";
        }

        inline std::string visitCompound(Carrot::CompoundExpression& expression) override {
            std::string lines;
            for (auto& pExpr : expression.getSubExpressions()) {
                lines += visit(pExpr);
                lines += "\n";
            }
            return lines;
        }

        inline std::string visitMod(Carrot::ModExpression& expression) override {
            return "(" + visit(expression.getOperand1()) + ") % (" + visit(expression.getOperand2()) + ")";
        }

        inline std::string visitMin(Carrot::MinExpression& expression) override {
            return "min(" + visit(expression.getOperand1()) + ", " + visit(expression.getOperand2()) + ")";
        }

        inline std::string visitMax(Carrot::MaxExpression& expression) override {
            return "max(" + visit(expression.getOperand1()) + ", " + visit(expression.getOperand2()) + ")";
        }

        inline std::string visitLess(Carrot::LessExpression& expression) override {
            return "(" + visit(expression.getOperand1()) + ") < (" + visit(expression.getOperand2()) + ")";
        }

        inline std::string visitLessOrEquals(Carrot::LessOrEqualsExpression& expression) override {
            return "(" + visit(expression.getOperand1()) + ") <= (" + visit(expression.getOperand2()) + ")";
        }

        inline std::string visitGreater(Carrot::GreaterExpression& expression) override {
            return "(" + visit(expression.getOperand1()) + ") > (" + visit(expression.getOperand2()) + ")";
        }

        inline std::string visitGreaterOrEquals(Carrot::GreaterOrEqualsExpression& expression) override {
            return "(" + visit(expression.getOperand1()) + ") >= (" + visit(expression.getOperand2()) + ")";
        }

        inline std::string visitEquals(Carrot::EqualsExpression& expression) override {
            return "(" + visit(expression.getOperand1()) + ") == (" + visit(expression.getOperand2()) + ")";
        }

        inline std::string visitNotEquals(Carrot::NotEqualsExpression& expression) override {
            return "(" + visit(expression.getOperand1()) + ") != (" + visit(expression.getOperand2()) + ")";
        }

        inline std::string visitOr(Carrot::OrExpression& expression) override {
            return "(" + visit(expression.getOperand1()) + ") | (" + visit(expression.getOperand2()) + ")";
        }

        inline std::string visitAnd(Carrot::AndExpression& expression) override {
            return "(" + visit(expression.getOperand1()) + ") & (" + visit(expression.getOperand2()) + ")";
        }

        inline std::string visitXor(Carrot::XorExpression& expression) override {
            return "(" + visit(expression.getOperand1()) + ") ^ (" + visit(expression.getOperand2()) + ")";
        }

        inline std::string visitBoolNegate(Carrot::BoolNegateExpression& expression) override {
            return "(!" + visit(expression.getOperand()) + ")";
        }

        inline std::string visitSin(Carrot::SinExpression& expression) override {
            return "sin(" + visit(expression.getOperand()) + ")";
        }

        inline std::string visitCos(Carrot::CosExpression& expression) override {
            return "cos(" + visit(expression.getOperand()) + ")";
        }
        inline std::string visitTan(Carrot::TanExpression& expression) override {
            return "tan(" + visit(expression.getOperand()) + ")";
        }
        inline std::string visitExp(Carrot::ExpExpression& expression) override {
            return "exp(" + visit(expression.getOperand()) + ")";
        }
        inline std::string visitAbs(Carrot::AbsExpression& expression) override {
            return "abs(" + visit(expression.getOperand()) + ")";
        }
        inline std::string visitSqrt(Carrot::SqrtExpression& expression) override {
            return "sqrt(" + visit(expression.getOperand()) + ")";
        }
        inline std::string visitLog(Carrot::LogExpression& expression) override {
            return "log(" + visit(expression.getOperand()) + ")";
        }

        inline std::string visitPlaceholder(Carrot::PlaceholderExpression& expression) override {
            throw std::runtime_error("Cannot accept placeholder expression!");
        }

        inline std::string visitOnce(Carrot::OnceExpression& expression) override {
            auto [iter, wasNew] = alreadyVisited.try_emplace(expression.getUUID());
            if (wasNew) {
                iter->second = visit(expression.getExpressionToExecute());
            }
            return "";
        }

        inline std::string visitPrefixed(Carrot::PrefixedExpression& expression) override {
            prefix += "\n";
            prefix += visit(expression.getPrefix()) + ";";
            return visit(expression.getExpression());
        }

        inline std::string visitSampleImage(Carrot::SampleImageExpression& expression) override {
            // 1. generate slot for image
            auto [iter, isNew] = imageIndices.try_emplace(expression.getImagePath());
            if (isNew) {
                u32 imageIndex = imageIndices.size();
                // TODO: allow selection of sampler
                generator.metadata.imageIndices[expression.getImagePath()] = imageIndex;

                iter->second = imageIndex;
            }

            const u32 imageId = iter->second;

            // 2. generate UV vector
            const std::string uv = "float2(" + visit(expression.getU()) + ", " + visit(expression.getV()) + ")";

            // 3. sample image
            // TODO: sampler
            return "particles.textures[" + std::to_string(imageId) + "].SampleLevel(particles.linearSampler, " + uv + ", 0)";
        }

        inline std::string visitGetVectorComponent(Carrot::GetVectorComponentExpression& expression) override {
            return "(" + visit(expression.getVectorInput()) + ")[" + std::to_string(expression.getComponentIndex()) + "]";
        }

        static void throwInvalidType(const Carrot::ExpressionType& expected, const Carrot::ExpressionType& actual, const std::string& where) {
            if(expected != actual) {
                throw std::runtime_error("Invalid type, expected " + expected.name() + ", got " + actual.name() + ", at " + where);
            }
        }

        inline std::string visitMakeVector(Carrot::MakeVectorExpression& expression) override {
            throwInvalidType(Carrot::ExpressionTypes::Float, expression.getInputs()[0]->getType(), "Unsupported type of element in vector");
            std::string result = "float" + std::to_string(expression.getVectorSize());
            result += '(';

            for (i32 i = 0; i < expression.getVectorSize(); i++) {
                if (i != 0) {
                    result += ", ";
                }
                result += visit(expression.getInputs()[i]);
            }

            result += ')';
            return result;
        }

        const std::string& getPrefix() const {
            return prefix;
        }

    private:
        ParticleShaderGenerator& generator;
        std::string prefix;
        std::unordered_map<Carrot::UUID, std::string> alreadyVisited;
        std::unordered_map<std::string, Id> imageIndices;
    };

    ParticleShaderGenerator::ParticleShaderGenerator(const std::string& projectName): projectName(projectName) {

    }

    CompiledParticleShaders ParticleShaderGenerator::compileToSPIRV(const ParticleShaderInputs& expressions) {
        std::string slangText = Carrot::IO::readFileAsText("./editor/shader_sources/particles_template.slang");

        std::string fragmentCode;
        std::string updateCode;

        for (auto& pExpression : expressions.fragment) {
            SlangVisitor visitor(*this);
            fragmentCode += visitor.visit(pExpression);
            fragmentCode += "\n";

            fragmentCode = visitor.getPrefix() + fragmentCode;
        }

        for (auto& pExpression : expressions.computeUpdate) {
            SlangVisitor visitor(*this);
            updateCode += visitor.visit(pExpression);
            updateCode += "\n";

            updateCode = visitor.getPrefix() + updateCode;
        }

        std::string slangOutput = Carrot::replace(slangText, "// MARKER TO REPLACE RENDER", fragmentCode);
        slangOutput = Carrot::replace(slangOutput, "// MARKER TO REPLACE UPDATE", updateCode);

        const std::filesystem::path tempFolder = Carrot::IO::getExecutablePath().parent_path() / ".temp" / "";
        if (!std::filesystem::exists(tempFolder)) {
            std::filesystem::create_directories(tempFolder);
        }
        const auto timestamp = std::chrono::system_clock::now();
        const u64 utcTime = timestamp.time_since_epoch().count();
        const std::filesystem::path tempShaderPath = tempFolder / Carrot::sprintf("%s-%llu.slang", projectName.c_str(), utcTime);

        //std::cout << "=== SLANG OUTPUT ===\n";
        //std::cout << slangOutput << std::endl;

        Carrot::IO::writeFile(tempShaderPath.string(), (void*)slangOutput.c_str(), slangOutput.size());
        CLEANUP(Carrot::IO::deleteFile(tempShaderPath));

        CompiledParticleShaders result;

        std::filesystem::path basePath = "./editor/shader_sources/";
        ShaderCompiler::FileIncluder includerFragment{basePath};
        ShaderCompiler::FileIncluder includerUpdate{basePath};
        if (0 != SlangCompiler::compileToSpirv(Carrot::toString(basePath.u8string()).c_str(), ShaderCompiler::Stage::Compute, "updateParticle", tempShaderPath, result.computeUpdate, includerUpdate)) {
            throw std::runtime_error("Failed to compile compute shader");
        }
        if (0 != SlangCompiler::compileToSpirv(Carrot::toString(basePath.u8string()).c_str(), ShaderCompiler::Stage::Fragment, "pixel", tempShaderPath, result.fragment, includerFragment)) {
            throw std::runtime_error("Failed to compile fragment shader");
        }

        std::vector<u32> vertexShader;
        if (0 != SlangCompiler::compileToSpirv(Carrot::toString(basePath.u8string()).c_str(), ShaderCompiler::Stage::Vertex, "vertex", tempShaderPath, vertexShader, includerFragment)) {
            throw std::runtime_error("Failed to compile vertex shader");
        }

        auto analyse = [&](const std::vector<uint32_t>& output)
        {
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
        };
        /*analyse(result.fragment);
        analyse(vertexShader);
        analyse(result.computeUpdate);*/

        return result;
    }

    const ParticleShadersMetadata& ParticleShaderGenerator::getMetadata() const {
        return metadata;
    }

}