//
// Created by jglrxavpok on 24/05/2021.
//

#pragma once

#include <node_based/EditorNode.h>
#include <node_based/nodes/BinaryFunctionNode.h>

namespace Fertilizer {

    struct LessNode: BinaryFunctionNode {
        explicit LessNode(Fertilizer::EditorGraph& graph): Fertilizer::BinaryFunctionNode(graph, "Less", "float_less", Carrot::ExpressionTypes::Bool) {}
        explicit LessNode(Fertilizer::EditorGraph& graph, const rapidjson::Value& json): Fertilizer::BinaryFunctionNode(graph, "Less", "float_less", Carrot::ExpressionTypes::Bool, json) {}

        std::shared_ptr<Carrot::Expression> toExpression(std::shared_ptr<Carrot::Expression> left, std::shared_ptr<Carrot::Expression> right) const override {
            return std::make_shared<Carrot::LessExpression>(left, right);
        }
    };

    struct LessOrEqualsNode: BinaryFunctionNode {
        explicit LessOrEqualsNode(Fertilizer::EditorGraph& graph): Fertilizer::BinaryFunctionNode(graph, "Less or Equals", "float_less_or_equals", Carrot::ExpressionTypes::Bool) {}
        explicit LessOrEqualsNode(Fertilizer::EditorGraph& graph, const rapidjson::Value& json): Fertilizer::BinaryFunctionNode(graph, "Less or Equals", "float_less_or_equals", Carrot::ExpressionTypes::Bool, json) {}

        std::shared_ptr<Carrot::Expression> toExpression(std::shared_ptr<Carrot::Expression> left, std::shared_ptr<Carrot::Expression> right) const override {
            return std::make_shared<Carrot::LessOrEqualsExpression>(left, right);
        }
    };

    struct GreaterNode: BinaryFunctionNode {
        explicit GreaterNode(Fertilizer::EditorGraph& graph): Fertilizer::BinaryFunctionNode(graph, "Greater", "float_greater", Carrot::ExpressionTypes::Bool) {}
        explicit GreaterNode(Fertilizer::EditorGraph& graph, const rapidjson::Value& json): Fertilizer::BinaryFunctionNode(graph, "Greater", "float_greater", Carrot::ExpressionTypes::Bool, json) {}

        std::shared_ptr<Carrot::Expression> toExpression(std::shared_ptr<Carrot::Expression> left, std::shared_ptr<Carrot::Expression> right) const override {
            return std::make_shared<Carrot::GreaterExpression>(left, right);
        }
    };

    struct GreaterOrEqualsNode: BinaryFunctionNode {
        explicit GreaterOrEqualsNode(Fertilizer::EditorGraph& graph): Fertilizer::BinaryFunctionNode(graph, "Greater or Equals", "float_greater_or_equals", Carrot::ExpressionTypes::Bool) {}
        explicit GreaterOrEqualsNode(Fertilizer::EditorGraph& graph, const rapidjson::Value& json): Fertilizer::BinaryFunctionNode(graph, "Greater or Equals", "float_greater_or_equals", Carrot::ExpressionTypes::Bool, json) {}

        std::shared_ptr<Carrot::Expression> toExpression(std::shared_ptr<Carrot::Expression> left, std::shared_ptr<Carrot::Expression> right) const override {
            return std::make_shared<Carrot::GreaterOrEqualsExpression>(left, right);
        }
    };

    struct EqualsNode: BinaryFunctionNode {
        explicit EqualsNode(Fertilizer::EditorGraph& graph): Fertilizer::BinaryFunctionNode(graph, "Equals", "float_equals", Carrot::ExpressionTypes::Bool) {}
        explicit EqualsNode(Fertilizer::EditorGraph& graph, const rapidjson::Value& json): Fertilizer::BinaryFunctionNode(graph, "Equals", "float_equals", Carrot::ExpressionTypes::Bool, json) {}

        std::shared_ptr<Carrot::Expression> toExpression(std::shared_ptr<Carrot::Expression> left, std::shared_ptr<Carrot::Expression> right) const override {
            return std::make_shared<Carrot::EqualsExpression>(left, right);
        }
    };

    struct NotEqualsNode: BinaryFunctionNode {
        explicit NotEqualsNode(Fertilizer::EditorGraph& graph): Fertilizer::BinaryFunctionNode(graph, "Not Equals", "float_not_equals", Carrot::ExpressionTypes::Bool) {}
        explicit NotEqualsNode(Fertilizer::EditorGraph& graph, const rapidjson::Value& json): Fertilizer::BinaryFunctionNode(graph, "Not Equals", "float_not_equals", Carrot::ExpressionTypes::Bool, json) {}

        std::shared_ptr<Carrot::Expression> toExpression(std::shared_ptr<Carrot::Expression> left, std::shared_ptr<Carrot::Expression> right) const override {
            return std::make_shared<Carrot::NotEqualsExpression>(left, right);
        }
    };

    class AndNode: public EditorNode {
    public:
        explicit AndNode(Fertilizer::EditorGraph& graph): Fertilizer::EditorNode(graph, "And", "and") {
            init();
        }

        explicit AndNode(Fertilizer::EditorGraph& graph, const rapidjson::Value& json): Fertilizer::EditorNode(graph, "And", "and", json) {
            init();
        }

        std::shared_ptr<Carrot::Expression> toExpression(uint32_t pinIndex, std::unordered_set<Carrot::UUID>& activeLinks) const override {
            auto inputExprs = getExpressionsFromInput(activeLinks);
            if (inputExprs[0] == nullptr || inputExprs[1] == nullptr)
                return nullptr;
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
        explicit OrNode(Fertilizer::EditorGraph& graph): Fertilizer::EditorNode(graph, "Or", "or") {
            init();
        }

        explicit OrNode(Fertilizer::EditorGraph& graph, const rapidjson::Value& json): Fertilizer::EditorNode(graph, "Or", "or", json) {
            init();
        }

        std::shared_ptr<Carrot::Expression> toExpression(uint32_t pinIndex, std::unordered_set<Carrot::UUID>& activeLinks) const override {
            auto inputExprs = getExpressionsFromInput(activeLinks);
            if (inputExprs[0] == nullptr || inputExprs[1] == nullptr)
                return nullptr;
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
        explicit XorNode(Fertilizer::EditorGraph& graph): Fertilizer::EditorNode(graph, "Xor", "xor") {
            init();
        }

        explicit XorNode(Fertilizer::EditorGraph& graph, const rapidjson::Value& json): Fertilizer::EditorNode(graph, "Xor", "xor", json) {
            init();
        }

        std::shared_ptr<Carrot::Expression> toExpression(uint32_t pinIndex, std::unordered_set<Carrot::UUID>& activeLinks) const override {
            auto inputExprs = getExpressionsFromInput(activeLinks);
            if (inputExprs[0] == nullptr || inputExprs[1] == nullptr)
                return nullptr;
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
        explicit NegateBoolNode(Fertilizer::EditorGraph& graph): Fertilizer::EditorNode(graph, "Negate", "not") {
            init();
        }

        explicit NegateBoolNode(Fertilizer::EditorGraph& graph, const rapidjson::Value& json): Fertilizer::EditorNode(graph, "Negate", "not", json) {
            init();
        }

        std::shared_ptr<Carrot::Expression> toExpression(uint32_t pinIndex, std::unordered_set<Carrot::UUID>& activeLinks) const override {
            auto inputExprs = getExpressionsFromInput(activeLinks);
            if (inputExprs[0] == nullptr)
                return nullptr;
            return std::make_shared<Carrot::BoolNegateExpression>(inputExprs[0]);
        }

    private:
        void init() {
            newInput("Op", Carrot::ExpressionTypes::Bool);

            newOutput("Result", Carrot::ExpressionTypes::Bool);
        }
    };
}