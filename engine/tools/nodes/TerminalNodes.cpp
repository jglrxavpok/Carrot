//
// Created by jglrxavpok on 11/05/2021.
//

#include "TerminalNodes.h"

#include <core/utils/ImGuiUtils.hpp>

namespace Tools {
    std::string TerminalNode::getTitle(TerminalNodeType type) {
        switch (type) {
            case TerminalNodeType::SetVelocity:
                return "Set Velocity";
            case TerminalNodeType::SetSize:
                return "Set Size";
            case TerminalNodeType::SetOutputColor:
                return "Set Output Color";
            case TerminalNodeType::DiscardPixel:
                return "Discard Pixel";
        }
        throw std::runtime_error("Unsupported terminal node type");
    }

    std::string TerminalNode::getInternalName(TerminalNodeType type) {
        switch (type) {
            case TerminalNodeType::SetVelocity:
                return "set_velocity";
            case TerminalNodeType::SetSize:
                return "set_size";
            case TerminalNodeType::SetOutputColor:
                return "set_output_color";
            case TerminalNodeType::DiscardPixel:
                return "discard_pixel";
        }
        throw std::runtime_error("Unsupported terminal node type");
    }

    uint32_t TerminalNode::getDimensionCount(TerminalNodeType type) {
        switch (type) {
            case TerminalNodeType::SetOutputColor:
                return 4;
            case TerminalNodeType::SetVelocity:
                return 3;
            case TerminalNodeType::DiscardPixel:
            case TerminalNodeType::SetSize:
                return 1;
        }
        throw std::runtime_error("Unsupported terminal node type");
    }

    Carrot::ExpressionType TerminalNode::getExpectedInputType(TerminalNodeType type, uint32_t inputIndex) {
        assert(inputIndex < getDimensionCount(type));
        switch (type) {
            case TerminalNodeType::SetOutputColor:
            case TerminalNodeType::SetVelocity:
            case TerminalNodeType::SetSize:
                return Carrot::ExpressionTypes::Float;

            case TerminalNodeType::DiscardPixel:
                return Carrot::ExpressionTypes::Bool;
        }
        throw std::runtime_error("Unsupported terminal node type");
    }

    TerminalNode::TerminalNode(Tools::EditorGraph& graph, TerminalNodeType type): Tools::EditorNode(graph, getTitle(type), getInternalName(type)), nodeType(type) {
        init();
    }

    TerminalNode::TerminalNode(Tools::EditorGraph& graph, TerminalNodeType type, const rapidjson::Value& json): Tools::EditorNode(graph, getTitle(type), getInternalName(type), json), nodeType(type) {
        init();
    }

    constexpr static const char* DIMENSION_NAMES[] = {
        "X", "Y", "Z", "W"
    };

    constexpr static const char* COLOR_DIMENSION_NAMES[] = {
        "R", "G", "B", "A"
    };

    constexpr static const char* BOOL_DIMENSION_NAMES[] = {
            "Condition", "Condition 2", "Condition 3", "Condition 4"
    };

    void TerminalNode::init() {
        const char* const* names = DIMENSION_NAMES;
        switch(nodeType) {
            case TerminalNodeType::SetOutputColor:
                names = COLOR_DIMENSION_NAMES;
                break;

            case TerminalNodeType::DiscardPixel:
                names = BOOL_DIMENSION_NAMES;
                break;

            default:
                break;
        }
        dimensions = getDimensionCount(nodeType);
        assert(dimensions <= 4);
        for(int i = 0; i < dimensions; i++) {
            newInput(names[i], getExpectedInputType(nodeType, i));
        }
    }

    std::shared_ptr<Carrot::Expression> TerminalNode::toExpression(uint32_t pinIndex, std::unordered_set<Carrot::UUID>& activeLinks) const {
        std::vector<std::shared_ptr<Carrot::Expression>> expressions;

        std::uint32_t index = 0;

        std::unordered_set<Carrot::UUID> activeLinksToThisNode;
        for(const auto& input : getExpressionsFromInput(activeLinksToThisNode)) {
            if(input != nullptr) {
                activeLinks.insert(activeLinksToThisNode.begin(), activeLinksToThisNode.end());
                activeLinksToThisNode.clear();
                expressions.push_back(std::make_shared<Carrot::SetVariableExpression>(getInternalName(nodeType), input, index));
            }

            index++;
        }
        return std::make_shared<Carrot::CompoundExpression>(std::move(expressions));
    }

    ImColor TerminalNode::getHeaderColor() const {
        return ImGuiUtils::getCarrotColor();
    }


}

