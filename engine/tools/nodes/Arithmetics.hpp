//
// Created by jglrxavpok on 03/05/2021.
//

#pragma once

#include <tools/EditorNode.h>
#include <tools/nodes/BinaryFunctionNode.h>

namespace Tools {
    struct AddNode: BinaryFunctionNode {
        explicit AddNode(Tools::EditorGraph& graph): Tools::BinaryFunctionNode(graph, "Add", "add", Carrot::ExpressionTypes::Float) {}
        explicit AddNode(Tools::EditorGraph& graph, const rapidjson::Value& json): Tools::BinaryFunctionNode(graph, "Add", "add", Carrot::ExpressionTypes::Float, json) {}

        std::shared_ptr<Carrot::Expression> toExpression(std::shared_ptr<Carrot::Expression> left, std::shared_ptr<Carrot::Expression> right) const override {
            return std::make_shared<Carrot::AddExpression>(left, right);
        }
    };

    struct SubNode: BinaryFunctionNode {
        explicit SubNode(Tools::EditorGraph& graph): Tools::BinaryFunctionNode(graph, "Sub", "sub", Carrot::ExpressionTypes::Float) {}
        explicit SubNode(Tools::EditorGraph& graph, const rapidjson::Value& json): Tools::BinaryFunctionNode(graph, "Sub", "sub", Carrot::ExpressionTypes::Float, json) {}

        std::shared_ptr<Carrot::Expression> toExpression(std::shared_ptr<Carrot::Expression> left, std::shared_ptr<Carrot::Expression> right) const override {
            return std::make_shared<Carrot::SubExpression>(left, right);
        }
    };

    struct MultNode: BinaryFunctionNode {
        explicit MultNode(Tools::EditorGraph& graph): Tools::BinaryFunctionNode(graph, "Mult", "mult", Carrot::ExpressionTypes::Float) {}
        explicit MultNode(Tools::EditorGraph& graph, const rapidjson::Value& json): Tools::BinaryFunctionNode(graph, "Mult", "mult", Carrot::ExpressionTypes::Float, json) {}

        std::shared_ptr<Carrot::Expression> toExpression(std::shared_ptr<Carrot::Expression> left, std::shared_ptr<Carrot::Expression> right) const override {
            return std::make_shared<Carrot::MultExpression>(left, right);
        }
    };

    struct DivNode: BinaryFunctionNode {
        explicit DivNode(Tools::EditorGraph& graph): Tools::BinaryFunctionNode(graph, "Div", "div", Carrot::ExpressionTypes::Float) {}
        explicit DivNode(Tools::EditorGraph& graph, const rapidjson::Value& json): Tools::BinaryFunctionNode(graph, "Div", "div", Carrot::ExpressionTypes::Float, json) {}

        std::shared_ptr<Carrot::Expression> toExpression(std::shared_ptr<Carrot::Expression> left, std::shared_ptr<Carrot::Expression> right) const override {
            return std::make_shared<Carrot::DivExpression>(left, right);
        }
    };

    struct ModNode: BinaryFunctionNode {
        explicit ModNode(Tools::EditorGraph& graph): Tools::BinaryFunctionNode(graph, "Modulus", "mod", Carrot::ExpressionTypes::Float) {}
        explicit ModNode(Tools::EditorGraph& graph, const rapidjson::Value& json): Tools::BinaryFunctionNode(graph, "Modulus", "mod", Carrot::ExpressionTypes::Float, json) {}

        std::shared_ptr<Carrot::Expression> toExpression(std::shared_ptr<Carrot::Expression> left, std::shared_ptr<Carrot::Expression> right) const override {
            return std::make_shared<Carrot::ModExpression>(left, right);
        }
    };

    struct MinNode: BinaryFunctionNode {
        explicit MinNode(Tools::EditorGraph& graph): Tools::BinaryFunctionNode(graph, "Min", "min", Carrot::ExpressionTypes::Float) {}
        explicit MinNode(Tools::EditorGraph& graph, const rapidjson::Value& json): Tools::BinaryFunctionNode(graph, "Min", "min", Carrot::ExpressionTypes::Float, json) {}

        std::shared_ptr<Carrot::Expression> toExpression(std::shared_ptr<Carrot::Expression> left, std::shared_ptr<Carrot::Expression> right) const override {
            return std::make_shared<Carrot::MinExpression>(left, right);
        }
    };

    struct MaxNode: BinaryFunctionNode {
        explicit MaxNode(Tools::EditorGraph& graph): Tools::BinaryFunctionNode(graph, "Max", "max", Carrot::ExpressionTypes::Float) {}
        explicit MaxNode(Tools::EditorGraph& graph, const rapidjson::Value& json): Tools::BinaryFunctionNode(graph, "Max", "max", Carrot::ExpressionTypes::Float, json) {}

        std::shared_ptr<Carrot::Expression> toExpression(std::shared_ptr<Carrot::Expression> left, std::shared_ptr<Carrot::Expression> right) const override {
            return std::make_shared<Carrot::MaxExpression>(left, right);
        }
    };
}
