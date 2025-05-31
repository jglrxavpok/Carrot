//
// Created by jglrxavpok on 11/05/2021.
//

#pragma once

#include <string>
#include <stdexcept>
#include "../EditorNode.h"
#include <core/expressions/Expressions.h>

#include <spirv.hpp>

namespace Tools {
    enum class TerminalNodeType {
        // Compute
        SetVelocity,
        SetSize,

        // Fragment
        SetOutputColor,
        DiscardPixel,

        // TODO
    };

    class TerminalNode: public EditorNode {
    private:
        TerminalNodeType nodeType;
        uint32_t dimensions = 0;

    public:
        static std::string getTitle(TerminalNodeType type);
        static std::string getInternalName(TerminalNodeType type);
        static uint32_t getDimensionCount(TerminalNodeType type);
        static Carrot::ExpressionType getExpectedInputType(TerminalNodeType type, uint32_t inputIndex);

        explicit TerminalNode(Tools::EditorGraph& graph, TerminalNodeType type);
        explicit TerminalNode(Tools::EditorGraph& graph, TerminalNodeType type, const rapidjson::Value& json);

    public:
        inline TerminalNodeType getTerminalType() const { return nodeType; };
        std::shared_ptr<Carrot::Expression> toExpression(uint32_t pinIndex, std::unordered_set<Carrot::UUID>& activeLinks) const override;
        ImColor getHeaderColor() const override;

    private:
        void init();
    };
}