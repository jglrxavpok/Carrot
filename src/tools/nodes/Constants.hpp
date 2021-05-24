//
// Created by jglrxavpok on 03/05/2021.
//

#pragma once

namespace Tools {
    class FloatConstantNode : public EditorNode {
    public:
        float value = 0.0f;

        explicit FloatConstantNode(Tools::EditorGraph& graph): Tools::FloatConstantNode(graph, 0.0f) {}

        explicit FloatConstantNode(Tools::EditorGraph& graph, float defaultValue): Tools::EditorNode(graph, "Float Constant", "float_constant"), value(defaultValue) {
            newOutput("Value", Carrot::ExpressionTypes::Float);
        }

        explicit FloatConstantNode(Tools::EditorGraph& graph, const rapidjson::Value& json): Tools::EditorNode(graph, "Float Constant", "float_constant", json) {
            newOutput("Value", Carrot::ExpressionTypes::Float);

            if(json.HasMember("extra")) {
                value = json["extra"]["value"].GetFloat();
            }
        }

        void renderCenter() override {
            ImGui::SetNextItemWidth(200);
            ImGui::DragFloat("##Value", &value, 1.0f);
        }

        rapidjson::Value serialiseToJSON(rapidjson::Document& doc) const override {
            return std::move(rapidjson::Value(rapidjson::kObjectType)
                        .AddMember("value", value, doc.GetAllocator())
            );
        }

        shared_ptr<Carrot::Expression> toExpression(uint32_t outputIndex) const override {
            return std::make_shared<Carrot::ConstantExpression>(value);
        }

    };

    class BoolConstantNode : public EditorNode {
    public:
        bool value = true;

        explicit BoolConstantNode(Tools::EditorGraph& graph): Tools::BoolConstantNode(graph, true) {}

        explicit BoolConstantNode(Tools::EditorGraph& graph, bool defaultValue): Tools::EditorNode(graph, "Bool Constant", "bool_constant"), value(defaultValue) {
            newOutput("Value", Carrot::ExpressionTypes::Bool);
        }

        explicit BoolConstantNode(Tools::EditorGraph& graph, const rapidjson::Value& json): Tools::EditorNode(graph, "Bool Constant", "bool_constant", json) {
            newOutput("Value", Carrot::ExpressionTypes::Bool);

            if(json.HasMember("extra")) {
                value = json["extra"]["value"].GetBool();
            }
        }

        void renderCenter() override {
            ImGui::SetNextItemWidth(200);
            ImGui::Checkbox("##Value", &value);
        }

        rapidjson::Value serialiseToJSON(rapidjson::Document& doc) const override {
            return std::move(rapidjson::Value(rapidjson::kObjectType)
                                     .AddMember("value", value, doc.GetAllocator())
            );
        }

        shared_ptr<Carrot::Expression> toExpression(uint32_t outputIndex) const override {
            return std::make_shared<Carrot::ConstantExpression>(value);
        }

    };
}