//
// Created by jglrxavpok on 26/05/2021.
//

#pragma once
#include <type_traits>
#include "../EditorNode.h"

namespace Fertilizer {
    template<typename Expr> requires std::is_base_of_v<Carrot::Expression, Expr>
    class FloatFunction: public EditorNode {
    public:
        explicit FloatFunction(Fertilizer::EditorGraph& graph, const std::string& title, const std::string& internalName): Fertilizer::EditorNode(graph, title, internalName) {
            init();
        }

        explicit FloatFunction(Fertilizer::EditorGraph& graph, const std::string& title, const std::string& internalName, const rapidjson::Value& json): Fertilizer::EditorNode(graph, title, internalName, json) {
            init();
        }

        std::shared_ptr<Carrot::Expression> toExpression(uint32_t pinIndex, std::unordered_set<Carrot::UUID>& activeLinks) const override {
            auto inputExprs = getExpressionsFromInput(activeLinks);
            if (inputExprs[0] == nullptr)
                return nullptr;
            return std::make_shared<Expr>(inputExprs[0]);
        }

    private:
        void init() {
            newInput("Op", Carrot::ExpressionTypes::Float);

            newOutput("Result", Carrot::ExpressionTypes::Float);
        }
    };

#define NODE(Type, InternalName)                                                                                                                    \
    class Type ## Node: public FloatFunction<Carrot:: Type ## Expression> {                                                                      \
    public:                                                                                                                                         \
        explicit Type ## Node(Fertilizer::EditorGraph& graph): Fertilizer::FloatFunction<Carrot:: Type ## Expression>(graph, #Type, #InternalName) {}      \
                                                                                                                                                    \
        explicit Type ## Node(Fertilizer::EditorGraph& graph, const rapidjson::Value& json): Fertilizer::FloatFunction<Carrot:: Type ## Expression>(graph, #Type, #InternalName, json) {}  \
    }                                                                                                                                               \

    NODE(Sin, sin);
    NODE(Cos, cos);
    NODE(Tan, tan);
    NODE(Exp, exp);
    NODE(Abs, abs);
    NODE(Sqrt, sqrt);
    NODE(Log, log);
#undef NODE
}