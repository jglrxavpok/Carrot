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
        SetVelocity,
        SetSize,
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
            }
            throw std::runtime_error("Unsupported terminal node type");
        }

        inline static std::string getInternalName(TerminalNodeType type) {
            switch (type) {
                case TerminalNodeType::SetVelocity:
                    return "set_velocity";
                case TerminalNodeType::SetSize:
                    return "set_size";
            }
            throw std::runtime_error("Unsupported terminal node type");
        }

        inline static uint32_t getDimensionCount(TerminalNodeType type) {
            switch (type) {
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

        void init() {
            dimensions = getDimensionCount(nodeType);
            assert(dimensions <= 4);
            for(int i = 0; i < dimensions; i++) {
                newInput(DIMENSION_NAMES[i]);
            }
        }
    };
}