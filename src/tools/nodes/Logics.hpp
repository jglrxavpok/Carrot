//
// Created by jglrxavpok on 24/05/2021.
//

#pragma once

#include "../EditorNode.h"

namespace Tools {
    class LessNode: public EditorNode {
    public:
        explicit LessNode(Tools::EditorGraph& graph): Tools::EditorNode(graph, "Less", "float_less") {
            init();
        }

        explicit LessNode(Tools::EditorGraph& graph, const rapidjson::Value& json): Tools::EditorNode(graph, "Less", "float_less", json) {
            init();
        }

        shared_ptr<Carrot::Expression> toExpression(uint32_t pinIndex) const override {
            auto inputExprs = getExpressionsFromInput();
            return std::make_shared<Carrot::LessExpression>(inputExprs[0], inputExprs[1]);
        }

    private:
        void init() {
            newInput("Op 1", Carrot::ExpressionTypes::Float);
            newInput("Op 2", Carrot::ExpressionTypes::Float);

            newOutput("Result", Carrot::ExpressionTypes::Bool);
        }
    };

    class LessOrEqualsNode: public EditorNode {
    public:
        explicit LessOrEqualsNode(Tools::EditorGraph& graph): Tools::EditorNode(graph, "Less or Equals", "float_less_or_equals") {
            init();
        }

        explicit LessOrEqualsNode(Tools::EditorGraph& graph, const rapidjson::Value& json): Tools::EditorNode(graph, "Less or Equals", "float_less_or_equals", json) {
            init();
        }

        shared_ptr<Carrot::Expression> toExpression(uint32_t pinIndex) const override {
            auto inputExprs = getExpressionsFromInput();
            return std::make_shared<Carrot::LessOrEqualsExpression>(inputExprs[0], inputExprs[1]);
        }

    private:
        void init() {
            newInput("Op 1", Carrot::ExpressionTypes::Float);
            newInput("Op 2", Carrot::ExpressionTypes::Float);

            newOutput("Result", Carrot::ExpressionTypes::Bool);
        }
    };

    class GreaterNode: public EditorNode {
    public:
        explicit GreaterNode(Tools::EditorGraph& graph): Tools::EditorNode(graph, "Greater", "float_greater") {
            init();
        }

        explicit GreaterNode(Tools::EditorGraph& graph, const rapidjson::Value& json): Tools::EditorNode(graph, "Greater", "float_greater", json) {
            init();
        }

        shared_ptr<Carrot::Expression> toExpression(uint32_t pinIndex) const override {
            auto inputExprs = getExpressionsFromInput();
            return std::make_shared<Carrot::GreaterExpression>(inputExprs[0], inputExprs[1]);
        }

    private:
        void init() {
            newInput("Op 1", Carrot::ExpressionTypes::Float);
            newInput("Op 2", Carrot::ExpressionTypes::Float);

            newOutput("Result", Carrot::ExpressionTypes::Bool);
        }
    };

    class GreaterOrEqualsNode: public EditorNode {
    public:
        explicit GreaterOrEqualsNode(Tools::EditorGraph& graph): Tools::EditorNode(graph, "Greater or Equals", "float_greater_or_equals") {
            init();
        }

        explicit GreaterOrEqualsNode(Tools::EditorGraph& graph, const rapidjson::Value& json): Tools::EditorNode(graph, "Greater or Equals", "float_greater_or_equals", json) {
            init();
        }

        shared_ptr<Carrot::Expression> toExpression(uint32_t pinIndex) const override {
            auto inputExprs = getExpressionsFromInput();
            return std::make_shared<Carrot::GreaterOrEqualsExpression>(inputExprs[0], inputExprs[1]);
        }

    private:
        void init() {
            newInput("Op 1", Carrot::ExpressionTypes::Float);
            newInput("Op 2", Carrot::ExpressionTypes::Float);

            newOutput("Result", Carrot::ExpressionTypes::Bool);
        }
    };

    class EqualsNode: public EditorNode {
    public:
        explicit EqualsNode(Tools::EditorGraph& graph): Tools::EditorNode(graph, "Equals", "float_equals") {
            init();
        }

        explicit EqualsNode(Tools::EditorGraph& graph, const rapidjson::Value& json): Tools::EditorNode(graph, "Equals", "float_equals", json) {
            init();
        }

        shared_ptr<Carrot::Expression> toExpression(uint32_t pinIndex) const override {
            auto inputExprs = getExpressionsFromInput();
            return std::make_shared<Carrot::EqualsExpression>(inputExprs[0], inputExprs[1]);
        }

    private:
        void init() {
            newInput("Op 1", Carrot::ExpressionTypes::Float);
            newInput("Op 2", Carrot::ExpressionTypes::Float);

            newOutput("Result", Carrot::ExpressionTypes::Bool);
        }
    };

    class NotEqualsNode: public EditorNode {
    public:
        explicit NotEqualsNode(Tools::EditorGraph& graph): Tools::EditorNode(graph, "Not Equals", "float_not_equals") {
            init();
        }

        explicit NotEqualsNode(Tools::EditorGraph& graph, const rapidjson::Value& json): Tools::EditorNode(graph, "Not Equals", "float_not_equals", json) {
            init();
        }

        shared_ptr<Carrot::Expression> toExpression(uint32_t pinIndex) const override {
            auto inputExprs = getExpressionsFromInput();
            return std::make_shared<Carrot::NotEqualsExpression>(inputExprs[0], inputExprs[1]);
        }

    private:
        void init() {
            newInput("Op 1", Carrot::ExpressionTypes::Float);
            newInput("Op 2", Carrot::ExpressionTypes::Float);

            newOutput("Result", Carrot::ExpressionTypes::Bool);
        }
    };

    class AndNode: public EditorNode {
    public:
        explicit AndNode(Tools::EditorGraph& graph): Tools::EditorNode(graph, "And", "and") {
            init();
        }

        explicit AndNode(Tools::EditorGraph& graph, const rapidjson::Value& json): Tools::EditorNode(graph, "And", "and", json) {
            init();
        }

        shared_ptr<Carrot::Expression> toExpression(uint32_t pinIndex) const override {
            auto inputExprs = getExpressionsFromInput();
            return std::make_shared<Carrot::AndExpression>(inputExprs[0], inputExprs[1]);
        }

    private:
        void init() {
            newInput("Op 1", Carrot::ExpressionTypes::Bool);
            newInput("Op 2", Carrot::ExpressionTypes::Bool);

            newOutput("Result", Carrot::ExpressionTypes::Bool);
        }
    };

    class OrNode: public EditorNode {
    public:
        explicit OrNode(Tools::EditorGraph& graph): Tools::EditorNode(graph, "Or", "or") {
            init();
        }

        explicit OrNode(Tools::EditorGraph& graph, const rapidjson::Value& json): Tools::EditorNode(graph, "Or", "or", json) {
            init();
        }

        shared_ptr<Carrot::Expression> toExpression(uint32_t pinIndex) const override {
            auto inputExprs = getExpressionsFromInput();
            return std::make_shared<Carrot::OrExpression>(inputExprs[0], inputExprs[1]);
        }

    private:
        void init() {
            newInput("Op 1", Carrot::ExpressionTypes::Bool);
            newInput("Op 2", Carrot::ExpressionTypes::Bool);

            newOutput("Result", Carrot::ExpressionTypes::Bool);
        }
    };

    class XorNode: public EditorNode {
    public:
        explicit XorNode(Tools::EditorGraph& graph): Tools::EditorNode(graph, "Xor", "xor") {
            init();
        }

        explicit XorNode(Tools::EditorGraph& graph, const rapidjson::Value& json): Tools::EditorNode(graph, "Xor", "xor", json) {
            init();
        }

        shared_ptr<Carrot::Expression> toExpression(uint32_t pinIndex) const override {
            auto inputExprs = getExpressionsFromInput();
            return std::make_shared<Carrot::XorExpression>(inputExprs[0], inputExprs[1]);
        }

    private:
        void init() {
            newInput("Op 1", Carrot::ExpressionTypes::Bool);
            newInput("Op 2", Carrot::ExpressionTypes::Bool);

            newOutput("Result", Carrot::ExpressionTypes::Bool);
        }
    };

    class NegateBoolNode: public EditorNode {
    public:
        explicit NegateBoolNode(Tools::EditorGraph& graph): Tools::EditorNode(graph, "Negate", "not") {
            init();
        }

        explicit NegateBoolNode(Tools::EditorGraph& graph, const rapidjson::Value& json): Tools::EditorNode(graph, "Negate", "not", json) {
            init();
        }

        shared_ptr<Carrot::Expression> toExpression(uint32_t pinIndex) const override {
            auto inputExprs = getExpressionsFromInput();
            return std::make_shared<Carrot::BoolNegateExpression>(inputExprs[0]);
        }

    private:
        void init() {
            newInput("Op", Carrot::ExpressionTypes::Bool);

            newOutput("Result", Carrot::ExpressionTypes::Bool);
        }
    };
}