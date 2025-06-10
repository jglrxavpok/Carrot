//
// Created by jglrxavpok on 30/05/25.
//

#include "BinaryFunctionNode.h"

#include <tools/EditorGraph.h>

namespace Tools {
    constexpr u8 VariantCount = 3;
    constexpr u8 TwoInputsVariant = 0;
    constexpr u8 InputAndConstantVariant = 1;
    constexpr u8 ConstantAndInputVariant = 2;

    BinaryFunctionNode::BinaryFunctionNode(EditorGraph& graph, const std::string& title, const std::string& internalName, const Carrot::ExpressionType& outputType)
        : NodeWithVariants(graph, title, internalName, VariantCount) {
        newOutput("Out", outputType);
        changeInputCount(2); // default variant is two inputs
    }

    BinaryFunctionNode::BinaryFunctionNode(EditorGraph& graph, const std::string& title, const std::string& internalName, const Carrot::ExpressionType& outputType, const rapidjson::Value& json)
        : NodeWithVariants(graph, title, internalName, VariantCount, json)
    {
        newOutput("Out", outputType);
        switch (currentVariant) {
        case TwoInputsVariant:
            changeInputCount(2);
            break;
        case InputAndConstantVariant:
        case ConstantAndInputVariant:
            changeInputCount(1);
            break;

        default:
            TODO;
        }

        if (auto extraMemberIt = json.FindMember("extra"); extraMemberIt != json.MemberEnd()) {
            if (auto constantMemberIt = extraMemberIt->value.FindMember("constant"); constantMemberIt != extraMemberIt->value.MemberEnd()) {
                constant = constantMemberIt->value.GetFloat();
            }
        }
    }

    std::shared_ptr<Carrot::Expression> BinaryFunctionNode::toExpression(uint32_t outputIndex, u8 variantIndex, std::unordered_set<Carrot::UUID>& activeLinks) const {
        verify(outputIndex == 0, "BinaryFunctionNode only has a single output");

        auto inputs = getExpressionsFromInput(activeLinks);
        switch (currentVariant) {
            case TwoInputsVariant: {
                verify(inputs.size() == 2, "Invalid input count");
                if (inputs[0] == nullptr || inputs[1] == nullptr) {
                    return nullptr;
                }
                return toExpression(inputs[0], inputs[1]);
            } break;

            case InputAndConstantVariant: {
                verify(inputs.size() == 1, "Invalid input count");
                if (inputs[0] == nullptr) {
                    return nullptr;
                }
                return toExpression(inputs[0], std::make_shared<Carrot::ConstantExpression>(constant));
            } break;

            case ConstantAndInputVariant: {
                verify(inputs.size() == 1, "Invalid input count");
                if (inputs[0] == nullptr) {
                    return nullptr;
                }
                return toExpression(std::make_shared<Carrot::ConstantExpression>(constant), inputs[0]);
            } break;

            default:
                TODO;
        }
    }

    bool BinaryFunctionNode::renderCenter(u8 variantIndex) {
        return false;
    }

    bool BinaryFunctionNode::renderHeaderWidgets(u8 variantIndex) {
        return EditorNode::renderHeaderWidgets(); // NOLINT(*-parent-virtual-call) (on purpose, otherwise infinite recursion)
    }

    rapidjson::Value BinaryFunctionNode::serialiseToJSON(rapidjson::MemoryPoolAllocator<>& allocator, u8 variantIndex) const {
        rapidjson::Value result{rapidjson::kObjectType};
        if (currentVariant == InputAndConstantVariant || currentVariant == ConstantAndInputVariant) {
            result.AddMember("constant", constant, allocator);
        }
        return result;
    }

    void BinaryFunctionNode::onVariantChanged(u8 oldVariant) {
        switch (currentVariant) {
            case TwoInputsVariant:
                changeInputCount(2);
                break;
            case InputAndConstantVariant:
            case ConstantAndInputVariant:
                changeInputCount(1);
                break;

            default:
                TODO;
        }
    }

    bool BinaryFunctionNode::renderInputPins(u8 variantIndex) {
        bool modified = false;
        constexpr float constantInputWidth = 50;
        switch (currentVariant) {
            case InputAndConstantVariant:
                modified |= EditorNode::renderInputPins();
                ImGui::Dummy(EditorGraph::PaddingDummySize);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(constantInputWidth);
                modified |= ImGui::DragFloat("Constant", &constant);
                break;
            case ConstantAndInputVariant:
                ImGui::Dummy(EditorGraph::PaddingDummySize);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(constantInputWidth);
                modified |= ImGui::DragFloat("Constant", &constant);
                modified |= EditorNode::renderInputPins();
                break;

            default:
                EditorNode::renderInputPins();
                break;
        }
        return modified;
    }

    bool BinaryFunctionNode::renderOutputPins(u8 variantIndex) {
        return EditorNode::renderOutputPins();
    }

    void BinaryFunctionNode::changeInputCount(u8 inputCount) {
        for (auto& input : inputs) {
            // TODO: be smarter and keep some links
            for (auto& link : graph.getLinksLeadingTo(*input)) {
                graph.removeLink(link);
            }
          //TODO:  graph.unregisterPin(input);
        }
        inputs.clear();

        for (u8 inputIndex = 0; inputIndex < inputCount; inputIndex++) {
            newInput(Carrot::sprintf("%u", inputIndex), Carrot::ExpressionTypes::Float);
        }
    }



} // Tools