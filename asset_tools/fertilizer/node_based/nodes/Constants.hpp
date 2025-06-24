//
// Created by jglrxavpok on 03/05/2021.
//

#pragma once

namespace Fertilizer {
    class FloatConstantNode : public EditorNode {
    public:
        float value = 0.0f;

        explicit FloatConstantNode(Fertilizer::EditorGraph& graph): Fertilizer::FloatConstantNode(graph, 0.0f) {}

        explicit FloatConstantNode(Fertilizer::EditorGraph& graph, float defaultValue): Fertilizer::EditorNode(graph, "Float Constant", "float_constant"), value(defaultValue) {
            newOutput("Value", Carrot::ExpressionTypes::Float);
        }

        explicit FloatConstantNode(Fertilizer::EditorGraph& graph, const rapidjson::Value& json): Fertilizer::EditorNode(graph, "Float Constant", "float_constant", json) {
            newOutput("Value", Carrot::ExpressionTypes::Float);

            if(json.HasMember("extra")) {
                value = json["extra"]["value"].GetFloat();
            }
        }

        bool renderCenter() override {
            ImGui::SetNextItemWidth(50);
            return ImGui::DragFloat("##Value", &value, 1.0f);
        }

        rapidjson::Value serialiseToJSON(rapidjson::MemoryPoolAllocator<>& allocator) const override {
            return std::move(rapidjson::Value(rapidjson::kObjectType)
                        .AddMember("value", value, allocator)
            );
        }

        std::shared_ptr<Carrot::Expression> toExpression(uint32_t outputIndex, std::unordered_set<Carrot::UUID>& activeLinks) const override {
            return std::make_shared<Carrot::ConstantExpression>(value);
        }

    };

    class BoolConstantNode : public EditorNode {
    public:
        bool value = true;

        explicit BoolConstantNode(Fertilizer::EditorGraph& graph): Fertilizer::BoolConstantNode(graph, true) {}

        explicit BoolConstantNode(Fertilizer::EditorGraph& graph, bool defaultValue): Fertilizer::EditorNode(graph, "Bool Constant", "bool_constant"), value(defaultValue) {
            newOutput("Value", Carrot::ExpressionTypes::Bool);
        }

        explicit BoolConstantNode(Fertilizer::EditorGraph& graph, const rapidjson::Value& json): Fertilizer::EditorNode(graph, "Bool Constant", "bool_constant", json) {
            newOutput("Value", Carrot::ExpressionTypes::Bool);

            if(json.HasMember("extra")) {
                value = json["extra"]["value"].GetBool();
            }
        }

        bool renderCenter() override {
            ImGui::SetNextItemWidth(50);
            return ImGui::Checkbox("##Value", &value);
        }

        rapidjson::Value serialiseToJSON(rapidjson::MemoryPoolAllocator<>& allocator) const override {
            return std::move(rapidjson::Value(rapidjson::kObjectType)
                                     .AddMember("value", value, allocator)
            );
        }

        std::shared_ptr<Carrot::Expression> toExpression(uint32_t outputIndex, std::unordered_set<Carrot::UUID>& activeLinks) const override {
            return std::make_shared<Carrot::ConstantExpression>(value);
        }

    };
}