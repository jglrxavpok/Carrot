//
// Created by jglrxavpok on 11/05/2021.
//

#pragma once

#include <string>
#include <stdexcept>
#include "../EditorNode.h"
#include <engine/expressions/Expressions.h>

#include <spirv.hpp>

namespace Tools {
    enum class TerminalNodeType {
        // Compute
        SetVelocity,
        SetSize,

        // Fragment
        SetOutputColor,

        // TODO
    };

    class TerminalNode: public EditorNode {
    private:
        TerminalNodeType nodeType;
        uint32_t dimensions = 0;

    public:
        explicit TerminalNode(Tools::EditorGraph& graph, TerminalNodeType type): Tools::EditorNode(graph, getTitle(type), getInternalName(type)), nodeType(type) {
            init();
        }

        explicit TerminalNode(Tools::EditorGraph& graph, TerminalNodeType type, const rapidjson::Value& json): Tools::EditorNode(graph, getTitle(type), getInternalName(type), json), nodeType(type) {
            init();
        }

    public:
        inline TerminalNodeType getTerminalType() const { return nodeType; };
        std::shared_ptr<Carrot::Expression> toExpression(uint32_t pinIndex) const override;

        inline static std::string getTitle(TerminalNodeType type) {
            switch (type) {
                case TerminalNodeType::SetVelocity:
                    return "Set Velocity";
                case TerminalNodeType::SetSize:
                    return "Set Size";
                case TerminalNodeType::SetOutputColor:
                    return "Set Output Color";
            }
            throw std::runtime_error("Unsupported terminal node type");
        }

        inline static std::string getInternalName(TerminalNodeType type) {
            switch (type) {
                case TerminalNodeType::SetVelocity:
                    return "set_velocity";
                case TerminalNodeType::SetSize:
                    return "set_size";
                case TerminalNodeType::SetOutputColor:
                    return "set_output_color";
            }
            throw std::runtime_error("Unsupported terminal node type");
        }

        inline static uint32_t getDimensionCount(TerminalNodeType type) {
            switch (type) {
                case TerminalNodeType::SetOutputColor:
                    return 4;
                case TerminalNodeType::SetVelocity:
                    return 3;
                case TerminalNodeType::SetSize:
                    return 1;
            }
            throw std::runtime_error("Unsupported terminal node type");
        }

    private:
        inline static const char* DIMENSION_NAMES[] = {
            "X", "Y", "Z", "W"
        };

        inline static const char* COLOR_DIMENSION_NAMES[] = {
            "R", "G", "B", "A"
        };

        void init() {
            const char** names = DIMENSION_NAMES;
            switch(nodeType) {
                case TerminalNodeType::SetOutputColor:
                    names = COLOR_DIMENSION_NAMES;
                    break;

                default:
                    break;
            }
            dimensions = getDimensionCount(nodeType);
            assert(dimensions <= 4);
            for(int i = 0; i < dimensions; i++) {
                newInput(names[i]);
            }
        }
    };
}