//
// Created by jglrxavpok on 11/05/2021.
//

#pragma once

#include <string>
#include <stdexcept>
#include "../EditorNode.h"
#include <engine/expressions/Expressions.h>

namespace Tools {
    enum class TerminalNodeType {
        SetVelocity,
        // TODO
    };

    class TerminalNode: public EditorNode {
    private:
        TerminalNodeType nodeType;

    public:
        explicit TerminalNode(Tools::EditorGraph& graph, TerminalNodeType type): Tools::EditorNode(graph, getTitle(type), getInternalName(type)), nodeType(type) {
            init();
        }

        explicit TerminalNode(Tools::EditorGraph& graph, TerminalNodeType type, const rapidjson::Value& json): Tools::EditorNode(graph, getTitle(type), getInternalName(type), json), nodeType(type) {
            init();
        }

    public:
        inline TerminalNodeType getTerminalType() const { return nodeType; };
        std::shared_ptr<Carrot::Expression> toExpression() const override;

        inline static std::string getTitle(TerminalNodeType type) {
            switch (type) {
                case TerminalNodeType::SetVelocity:
                    return "Set Velocity";
            }
            throw std::runtime_error("Unsupported terminal node type");
        }

        inline static std::string getInternalName(TerminalNodeType type) {
            switch (type) {
                case TerminalNodeType::SetVelocity:
                    return "set_velocity";
            }
            throw std::runtime_error("Unsupported terminal node type");
        }

    private:
        void init() {
            newInput("Result");
        }
    };
}