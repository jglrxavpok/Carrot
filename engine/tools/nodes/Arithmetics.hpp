//
// Created by jglrxavpok on 03/05/2021.
//

#pragma once

#include "../EditorNode.h"

namespace Tools {
    class AddNode: public EditorNode {
    public:
        explicit AddNode(Tools::EditorGraph& graph): Tools::EditorNode(graph, "Add", "add") {
            init();
        }

        explicit AddNode(Tools::EditorGraph& graph, const rapidjson::Value& json): Tools::EditorNode(graph, "Add", "add", json) {
            init();
        }

        std::shared_ptr<Carrot::Expression> toExpression(uint32_t pinIndex) const override {
            auto inputExprs = getExpressionsFromInput();
            return std::make_shared<Carrot::AddExpression>(inputExprs[0], inputExprs[1]);
        }

    private:
        void init() {
            newInput("Op 1", Carrot::ExpressionTypes::Float);
            newInput("Op 2", Carrot::ExpressionTypes::Float);

            newOutput("Result", Carrot::ExpressionTypes::Float);
        }
    };

    class SubNode: public EditorNode {
    public:
        explicit SubNode(Tools::EditorGraph& graph): Tools::EditorNode(graph, "Subtract", "sub") {
            init();
        }

        explicit SubNode(Tools::EditorGraph& graph, const rapidjson::Value& json): Tools::EditorNode(graph, "Subtract", "sub", json) {
            init();
        }

        std::shared_ptr<Carrot::Expression> toExpression(uint32_t pinIndex) const override {
            auto inputExprs = getExpressionsFromInput();
            return std::make_shared<Carrot::SubExpression>(inputExprs[0], inputExprs[1]);
        }

    private:
        void init() {
            newInput("Op 1", Carrot::ExpressionTypes::Float);
            newInput("Op 2", Carrot::ExpressionTypes::Float);

            newOutput("Result", Carrot::ExpressionTypes::Float);
        }
    };

    class MultNode: public EditorNode {
    public:
        explicit MultNode(Tools::EditorGraph& graph): Tools::EditorNode(graph, "Multiply", "mult") {
            init();
        }

        explicit MultNode(Tools::EditorGraph& graph, const rapidjson::Value& json): Tools::EditorNode(graph, "Multiply", "mult", json) {
            init();
        }

        std::shared_ptr<Carrot::Expression> toExpression(uint32_t pinIndex) const override {
            auto inputExprs = getExpressionsFromInput();
            return std::make_shared<Carrot::MultExpression>(inputExprs[0], inputExprs[1]);
        }

    private:
        void init() {
            newInput("Op 1", Carrot::ExpressionTypes::Float);
            newInput("Op 2", Carrot::ExpressionTypes::Float);

            newOutput("Result", Carrot::ExpressionTypes::Float);
        }
    };

    class DivNode: public EditorNode {
    public:
        explicit DivNode(Tools::EditorGraph& graph): Tools::EditorNode(graph, "Divide", "div") {
            init();
        }

        explicit DivNode(Tools::EditorGraph& graph, const rapidjson::Value& json): Tools::EditorNode(graph, "Divide", "div", json) {
            init();
        }

        std::shared_ptr<Carrot::Expression> toExpression(uint32_t pinIndex) const override {
            auto inputExprs = getExpressionsFromInput();
            return std::make_shared<Carrot::DivExpression>(inputExprs[0], inputExprs[1]);
        }

    private:
        void init() {
            newInput("Op 1", Carrot::ExpressionTypes::Float);
            newInput("Op 2", Carrot::ExpressionTypes::Float);

            newOutput("Result", Carrot::ExpressionTypes::Float);
        }
    };

    class ModNode: public EditorNode {
    public:
        explicit ModNode(Tools::EditorGraph& graph): Tools::EditorNode(graph, "Modulus", "mod") {
            init();
        }

        explicit ModNode(Tools::EditorGraph& graph, const rapidjson::Value& json): Tools::EditorNode(graph, "Modulus", "mod", json) {
            init();
        }

        std::shared_ptr<Carrot::Expression> toExpression(uint32_t pinIndex) const override {
            auto inputExprs = getExpressionsFromInput();
            return std::make_shared<Carrot::ModExpression>(inputExprs[0], inputExprs[1]);
        }

    private:
        void init() {
            newInput("Op 1", Carrot::ExpressionTypes::Float);
            newInput("Op 2", Carrot::ExpressionTypes::Float);

            newOutput("Result", Carrot::ExpressionTypes::Float);
        }
    };

    class MinNode: public EditorNode {
    public:
        explicit MinNode(Tools::EditorGraph& graph): Tools::EditorNode(graph, "Min", "min") {
            init();
        }

        explicit MinNode(Tools::EditorGraph& graph, const rapidjson::Value& json): Tools::EditorNode(graph, "Min", "min", json) {
            init();
        }

        std::shared_ptr<Carrot::Expression> toExpression(uint32_t pinIndex) const override {
            auto inputExprs = getExpressionsFromInput();
            return std::make_shared<Carrot::MinExpression>(inputExprs[0], inputExprs[1]);
        }

    private:
        void init() {
            newInput("Op 1", Carrot::ExpressionTypes::Float);
            newInput("Op 2", Carrot::ExpressionTypes::Float);

            newOutput("Result", Carrot::ExpressionTypes::Float);
        }
    };

    class MaxNode: public EditorNode {
    public:
        explicit MaxNode(Tools::EditorGraph& graph): Tools::EditorNode(graph, "Max", "max") {
            init();
        }

        explicit MaxNode(Tools::EditorGraph& graph, const rapidjson::Value& json): Tools::EditorNode(graph, "Max", "max", json) {
            init();
        }

        std::shared_ptr<Carrot::Expression> toExpression(uint32_t pinIndex) const override {
            auto inputExprs = getExpressionsFromInput();
            return std::make_shared<Carrot::MaxExpression>(inputExprs[0], inputExprs[1]);
        }

    private:
        void init() {
            newInput("Op 1", Carrot::ExpressionTypes::Float);
            newInput("Op 2", Carrot::ExpressionTypes::Float);

            newOutput("Result", Carrot::ExpressionTypes::Float);
        }
    };
}
