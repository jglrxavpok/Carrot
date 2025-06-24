//
// Created by jglrxavpok on 03/05/2021.
//

#pragma once

#include <node_based/EditorNode.h>
#include <node_based/nodes/BinaryFunctionNode.h>

namespace Fertilizer {
    struct AddNode: BinaryFunctionNode {
        explicit AddNode(Fertilizer::EditorGraph& graph): Fertilizer::BinaryFunctionNode(graph, "Add", "add", Carrot::ExpressionTypes::Float) {}
        explicit AddNode(Fertilizer::EditorGraph& graph, const rapidjson::Value& json): Fertilizer::BinaryFunctionNode(graph, "Add", "add", Carrot::ExpressionTypes::Float, json) {}

        std::shared_ptr<Carrot::Expression> toExpression(std::shared_ptr<Carrot::Expression> left, std::shared_ptr<Carrot::Expression> right) const override {
            return std::make_shared<Carrot::AddExpression>(left, right);
        }
    };

    struct SubNode: BinaryFunctionNode {
        explicit SubNode(Fertilizer::EditorGraph& graph): Fertilizer::BinaryFunctionNode(graph, "Sub", "sub", Carrot::ExpressionTypes::Float) {}
        explicit SubNode(Fertilizer::EditorGraph& graph, const rapidjson::Value& json): Fertilizer::BinaryFunctionNode(graph, "Sub", "sub", Carrot::ExpressionTypes::Float, json) {}

        std::shared_ptr<Carrot::Expression> toExpression(std::shared_ptr<Carrot::Expression> left, std::shared_ptr<Carrot::Expression> right) const override {
            return std::make_shared<Carrot::SubExpression>(left, right);
        }
    };

    struct MultNode: BinaryFunctionNode {
        explicit MultNode(Fertilizer::EditorGraph& graph): Fertilizer::BinaryFunctionNode(graph, "Mult", "mult", Carrot::ExpressionTypes::Float) {}
        explicit MultNode(Fertilizer::EditorGraph& graph, const rapidjson::Value& json): Fertilizer::BinaryFunctionNode(graph, "Mult", "mult", Carrot::ExpressionTypes::Float, json) {}

        std::shared_ptr<Carrot::Expression> toExpression(std::shared_ptr<Carrot::Expression> left, std::shared_ptr<Carrot::Expression> right) const override {
            return std::make_shared<Carrot::MultExpression>(left, right);
        }
    };

    struct DivNode: BinaryFunctionNode {
        explicit DivNode(Fertilizer::EditorGraph& graph): Fertilizer::BinaryFunctionNode(graph, "Div", "div", Carrot::ExpressionTypes::Float) {}
        explicit DivNode(Fertilizer::EditorGraph& graph, const rapidjson::Value& json): Fertilizer::BinaryFunctionNode(graph, "Div", "div", Carrot::ExpressionTypes::Float, json) {}

        std::shared_ptr<Carrot::Expression> toExpression(std::shared_ptr<Carrot::Expression> left, std::shared_ptr<Carrot::Expression> right) const override {
            return std::make_shared<Carrot::DivExpression>(left, right);
        }
    };

    struct ModNode: BinaryFunctionNode {
        explicit ModNode(Fertilizer::EditorGraph& graph): Fertilizer::BinaryFunctionNode(graph, "Modulus", "mod", Carrot::ExpressionTypes::Float) {}
        explicit ModNode(Fertilizer::EditorGraph& graph, const rapidjson::Value& json): Fertilizer::BinaryFunctionNode(graph, "Modulus", "mod", Carrot::ExpressionTypes::Float, json) {}

        std::shared_ptr<Carrot::Expression> toExpression(std::shared_ptr<Carrot::Expression> left, std::shared_ptr<Carrot::Expression> right) const override {
            return std::make_shared<Carrot::ModExpression>(left, right);
        }
    };

    struct MinNode: BinaryFunctionNode {
        explicit MinNode(Fertilizer::EditorGraph& graph): Fertilizer::BinaryFunctionNode(graph, "Min", "min", Carrot::ExpressionTypes::Float) {}
        explicit MinNode(Fertilizer::EditorGraph& graph, const rapidjson::Value& json): Fertilizer::BinaryFunctionNode(graph, "Min", "min", Carrot::ExpressionTypes::Float, json) {}

        std::shared_ptr<Carrot::Expression> toExpression(std::shared_ptr<Carrot::Expression> left, std::shared_ptr<Carrot::Expression> right) const override {
            return std::make_shared<Carrot::MinExpression>(left, right);
        }
    };

    struct MaxNode: BinaryFunctionNode {
        explicit MaxNode(Fertilizer::EditorGraph& graph): Fertilizer::BinaryFunctionNode(graph, "Max", "max", Carrot::ExpressionTypes::Float) {}
        explicit MaxNode(Fertilizer::EditorGraph& graph, const rapidjson::Value& json): Fertilizer::BinaryFunctionNode(graph, "Max", "max", Carrot::ExpressionTypes::Float, json) {}

        std::shared_ptr<Carrot::Expression> toExpression(std::shared_ptr<Carrot::Expression> left, std::shared_ptr<Carrot::Expression> right) const override {
            return std::make_shared<Carrot::MaxExpression>(left, right);
        }
    };
}
