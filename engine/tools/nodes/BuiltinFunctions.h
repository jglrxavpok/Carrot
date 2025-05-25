//
// Created by jglrxavpok on 26/05/2021.
//

#pragma once
#include <type_traits>
#include "../EditorNode.h"

namespace Tools {
    template<typename Expr> requires std::is_base_of_v<Carrot::Expression, Expr>
    class FloatFunction: public EditorNode {
    public:
        explicit FloatFunction(Tools::EditorGraph& graph, const std::string& title, const std::string& internalName): Tools::EditorNode(graph, title, internalName) {
            init();
        }

        explicit FloatFunction(Tools::EditorGraph& graph, const std::string& title, const std::string& internalName, const rapidjson::Value& json): Tools::EditorNode(graph, title, internalName, json) {
            init();
        }

        std::shared_ptr<Carrot::Expression> toExpression(uint32_t pinIndex) const override {
            auto inputExprs = getExpressionsFromInput();
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
        explicit Type ## Node(Tools::EditorGraph& graph): Tools::FloatFunction<Carrot:: Type ## Expression>(graph, #Type, #InternalName) {}      \
                                                                                                                                                    \
        explicit Type ## Node(Tools::EditorGraph& graph, const rapidjson::Value& json): Tools::FloatFunction<Carrot:: Type ## Expression>(graph, #Type, #InternalName, json) {}  \
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