//
// Created by jglrxavpok on 10/06/2021.
//

#include "Template.h"
#include "tools/EditorGraph.h"
#include "tools/nodes/NamedIO.h"
#include "engine/io/Logging.hpp"
#include "engine/expressions/ExpressionVisitor.h"
#include "tools/ParticleEditor.h"

namespace Tools {
    class PlaceholderRemapper: public Carrot::IExpressionVisitor<std::shared_ptr<Carrot::Expression>> {
    public:
        explicit PlaceholderRemapper(const std::unordered_map<std::string, std::vector<std::size_t>>& inputPins,
                                     const std::vector<std::shared_ptr<Carrot::Expression>>& inputExpressions)
                                     : Carrot::IExpressionVisitor<std::shared_ptr<Carrot::Expression>>(),
                                        inputPins(inputPins),
                                        inputExpressions(inputExpressions)
                                        {};

        std::shared_ptr<Carrot::Expression> visitConstant(Carrot::ConstantExpression& expression) override {
            return expression.shared_from_this();
        }

        std::shared_ptr<Carrot::Expression> visitGetVariable(Carrot::GetVariableExpression& expression) override {
            return expression.shared_from_this();
        }

        std::shared_ptr<Carrot::Expression> visitSetVariable(Carrot::SetVariableExpression& expression) override {
            return std::make_shared<Carrot::SetVariableExpression>(expression.getVariableName(), visit(expression.getValue()));
        }

        std::shared_ptr<Carrot::Expression> visitAdd(Carrot::AddExpression& expression) override {
            return std::make_shared<Carrot::AddExpression>(visit(expression.getOperand1()), visit(expression.getOperand2()));
        }

        std::shared_ptr<Carrot::Expression> visitSub(Carrot::SubExpression& expression) override {
            return std::make_shared<Carrot::SubExpression>(visit(expression.getOperand1()), visit(expression.getOperand2()));
        }

        std::shared_ptr<Carrot::Expression> visitMult(Carrot::MultExpression& expression) override {
            return std::make_shared<Carrot::MultExpression>(visit(expression.getOperand1()), visit(expression.getOperand2()));
        }

        std::shared_ptr<Carrot::Expression> visitDiv(Carrot::DivExpression& expression) override {
            return std::make_shared<Carrot::DivExpression>(visit(expression.getOperand1()), visit(expression.getOperand2()));
        }

        std::shared_ptr<Carrot::Expression> visitCompound(Carrot::CompoundExpression& expression) override {
            auto& subExprs = expression.getSubExpressions();
            std::vector<std::shared_ptr<Carrot::Expression>> expressions(subExprs.size());

            for (int i = 0; i < subExprs.size(); ++i) {
                expressions[i] = visit(subExprs[i]);
            }
            return std::make_shared<Carrot::CompoundExpression>(expressions);
        }

        std::shared_ptr<Carrot::Expression> visitMod(Carrot::ModExpression& expression) override {
            return std::make_shared<Carrot::ModExpression>(visit(expression.getOperand1()), visit(expression.getOperand2()));
        }

        std::shared_ptr<Carrot::Expression> visitLess(Carrot::LessExpression& expression) override {
            return std::make_shared<Carrot::LessExpression>(visit(expression.getOperand1()), visit(expression.getOperand2()));
        }

        std::shared_ptr<Carrot::Expression> visitLessOrEquals(Carrot::LessOrEqualsExpression& expression) override {
            return std::make_shared<Carrot::LessOrEqualsExpression>(visit(expression.getOperand1()), visit(expression.getOperand2()));
        }

        std::shared_ptr<Carrot::Expression> visitGreater(Carrot::GreaterExpression& expression) override {
            return std::make_shared<Carrot::GreaterExpression>(visit(expression.getOperand1()), visit(expression.getOperand2()));
        }

        std::shared_ptr<Carrot::Expression> visitGreaterOrEquals(Carrot::GreaterOrEqualsExpression& expression) override {
            return std::make_shared<Carrot::GreaterOrEqualsExpression>(visit(expression.getOperand1()), visit(expression.getOperand2()));
        }

        std::shared_ptr<Carrot::Expression> visitEquals(Carrot::EqualsExpression& expression) override {
            return std::make_shared<Carrot::EqualsExpression>(visit(expression.getOperand1()), visit(expression.getOperand2()));
        }

        std::shared_ptr<Carrot::Expression> visitNotEquals(Carrot::NotEqualsExpression& expression) override {
            return std::make_shared<Carrot::NotEqualsExpression>(visit(expression.getOperand1()), visit(expression.getOperand2()));
        }

        std::shared_ptr<Carrot::Expression> visitOr(Carrot::OrExpression& expression) override {
            return std::make_shared<Carrot::OrExpression>(visit(expression.getOperand1()), visit(expression.getOperand2()));
        }

        std::shared_ptr<Carrot::Expression> visitAnd(Carrot::AndExpression& expression) override {
            return std::make_shared<Carrot::AndExpression>(visit(expression.getOperand1()), visit(expression.getOperand2()));
        }

        std::shared_ptr<Carrot::Expression> visitXor(Carrot::XorExpression& expression) override {
            return std::make_shared<Carrot::XorExpression>(visit(expression.getOperand1()), visit(expression.getOperand2()));
        }

        std::shared_ptr<Carrot::Expression> visitBoolNegate(Carrot::BoolNegateExpression& expression) override {
            return std::make_shared<Carrot::BoolNegateExpression>(visit(expression.getOperand()));
        }

        std::shared_ptr<Carrot::Expression> visitSin(Carrot::SinExpression& expression) override {
            return std::make_shared<Carrot::SinExpression>(visit(expression.getOperand()));
        }

        std::shared_ptr<Carrot::Expression> visitCos(Carrot::CosExpression& expression) override {
            return std::make_shared<Carrot::CosExpression>(visit(expression.getOperand()));
        }

        std::shared_ptr<Carrot::Expression> visitTan(Carrot::TanExpression& expression) override {
            return std::make_shared<Carrot::TanExpression>(visit(expression.getOperand()));
        }

        std::shared_ptr<Carrot::Expression> visitExp(Carrot::ExpExpression& expression) override {
            return std::make_shared<Carrot::ExpExpression>(visit(expression.getOperand()));
        }

        std::shared_ptr<Carrot::Expression> visitAbs(Carrot::AbsExpression& expression) override {
            return std::make_shared<Carrot::AbsExpression>(visit(expression.getOperand()));
        }

        std::shared_ptr<Carrot::Expression> visitSqrt(Carrot::SqrtExpression& expression) override {
            return std::make_shared<Carrot::SqrtExpression>(visit(expression.getOperand()));
        }

        std::shared_ptr<Carrot::Expression> visitLog(Carrot::LogExpression& expression) override {
            return std::make_shared<Carrot::LogExpression>(visit(expression.getOperand()));
        }

        std::shared_ptr<Carrot::Expression> visitPlaceholder(Carrot::PlaceholderExpression& expression) override {
            auto placeholderData = std::static_pointer_cast<NamedInputPlaceholderData>(expression.getData());
            if(!placeholderData) {
                throw std::runtime_error("Unexpected placeholder data type");
            }

            auto pinsIt = inputPins.find(placeholderData->name);
            if(pinsIt == inputPins.end()) {
                throw std::runtime_error("Could not find corresponding pin indices: "+placeholderData->name);
            }

            auto start = pinsIt->second[0];
            auto offset = placeholderData->index;
            auto actualIndex = start + offset;
            return visit(inputExpressions[actualIndex]);
        }

    private:
        const std::vector<std::shared_ptr<Carrot::Expression>>& inputExpressions;
        const std::unordered_map<std::string, std::vector<std::size_t>>& inputPins;
    };

    void TemplateNode::loadInternalGraph(const rapidjson::Value& graphJson) {
        internalGraph = std::make_unique<EditorGraph>(graph.getEngine(), "template_internal_" + title);
        {
            NodeLibraryMenuScope s1("Operators", internalGraph.get());
            ParticleEditor::addCommonOperators(*internalGraph);
        }
        {
            NodeLibraryMenuScope s1("Logic", internalGraph.get());
            ParticleEditor::addCommonLogic(*internalGraph);
        }

        {
            NodeLibraryMenuScope s1("Functions", internalGraph.get());
            ParticleEditor::addCommonMath(*internalGraph);
        }

        {
            NodeLibraryMenuScope s1("Inputs", internalGraph.get());
            ParticleEditor::addCommonInputs(*internalGraph);
        }

        {
            NodeLibraryMenuScope s1("Update Inputs", internalGraph.get());
            internalGraph->addVariableToLibrary<VariableNodeType::GetDeltaTime>();
            internalGraph->addVariableToLibrary<VariableNodeType::GetFragmentPosition>();
        }
        {
            NodeLibraryMenuScope s3("Render/Update Outputs", internalGraph.get());
            internalGraph->addToLibrary<TerminalNodeType::SetOutputColor>();
            internalGraph->addToLibrary<TerminalNodeType::DiscardPixel>();
            internalGraph->addToLibrary<TerminalNodeType::SetVelocity>();
            internalGraph->addToLibrary<TerminalNodeType::SetSize>();
        }

        {
            NodeLibraryMenuScope s("Named I/O", internalGraph.get());
            internalGraph->addToLibrary<NamedInputNode>("named_input", "Named Input");
            internalGraph->addToLibrary<NamedOutputNode>("named_output", "Named Output");
        }

        internalGraph->addTemplatesToLibrary();
        internalGraph->addTemplateSupport();

        internalGraph->loadFromJSON(graphJson);
    }

    std::shared_ptr<Carrot::Expression> TemplateNode::toExpression(uint32_t outputIndex) const {
        //  find corresponding output inside internalGraph
        //  generate expression from internalGraph starting from output
        //  -> how to handle Named Inputs ?
        //  ----> find corresponding input on this node and go from there
        auto& outputPin = outputs[outputIndex];

        std::shared_ptr<Carrot::Expression> outputExpression = nullptr;
        std::vector<std::shared_ptr<Carrot::Expression>> inputExpressions{inputs.size()};

        auto templateInputs = getExpressionsFromInput();

        for(const auto& [id, node] : internalGraph->getNodes()) {
            if(auto output = std::dynamic_pointer_cast<NamedOutputNode>(node)) {
                auto pinsIt = outputPinIndices.find(output->getIOName());
                if(pinsIt == outputPinIndices.end()) {
                    throw std::runtime_error("Could not find pins corresponding to output "+output->getIOName());
                }
                auto& pins = pinsIt->second;
                if(std::find(WHOLE_CONTAINER(pins), outputPin->pinIndex) == pins.end()) {
                    // not this output, check others
                    continue;
                }

                auto startIndex = pins[0];
                auto coordinateOffset = outputIndex - startIndex;

                if(outputExpression) {
                    throw std::runtime_error("Found another output corresponding to the same named output node! Node is "+output->getIOName());
                }
                outputExpression = output->toExpression(coordinateOffset);
            }

            if(auto input = std::dynamic_pointer_cast<NamedInputNode>(node)) {
                auto pinsIt = inputPinIndices.find(input->getIOName());
                if(pinsIt == inputPinIndices.end()) {
                    throw std::runtime_error("Could not find pins corresponding to input "+input->getIOName());
                }
                auto& pins = pinsIt->second;

                for(const auto& pinIndex : pins) {
                    inputExpressions[pinIndex] = templateInputs[pinIndex];
                }
            }
        }

        auto internalExpressions = internalGraph->generateExpressionsFromTerminalNodes();

        PlaceholderRemapper remapper{inputPinIndices, inputExpressions};
        std::vector<std::shared_ptr<Carrot::Expression>> remappedInternalExpressions;
        for(const auto& expr : internalExpressions) {
            remappedInternalExpressions.push_back(remapper.visit(expr));
        }

        auto remappedOutput = remapper.visit(outputExpression);

        return std::make_shared<Carrot::PrefixedExpression>(
                    std::make_shared<Carrot::OnceExpression>(getID(), std::make_shared<Carrot::CompoundExpression>(remappedInternalExpressions)),
                    remappedOutput
                );
    }
}