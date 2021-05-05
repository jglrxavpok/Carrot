//
// Created by jglrxavpok on 03/05/2021.
//

#pragma once

namespace Tools {
    class FloatConstantNode : public EditorNode {
    public:
        float value = 0.0f;

        explicit FloatConstantNode(Tools::EditorGraph& graph): Tools::FloatConstantNode(graph, 0.0f) {}

        explicit FloatConstantNode(Tools::EditorGraph& graph, float defaultValue): Tools::EditorNode(graph, "Constant", "float_constant"), value(defaultValue) {
            newOutput("Value");
        }

        explicit FloatConstantNode(Tools::EditorGraph& graph, const rapidjson::Value& json): Tools::EditorNode(graph, "Constant", "float_constant", json) {
            newOutput("Value");

            if(json.HasMember("extra")) {
                value = json["extra"]["value"].GetFloat();
            }
        }

        void renderCenter() override {
            ImGui::SetNextItemWidth(200);
            ImGui::DragFloat("##Value", &value, 1.0f);
        }

        rapidjson::Value serialiseToJSON(rapidjson::Document& doc) override {
            return std::move(rapidjson::Value(rapidjson::kObjectType)
                        .AddMember("value", value, doc.GetAllocator())
            );
        }

    };
}