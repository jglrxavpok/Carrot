//
// Created by jglrxavpok on 18/04/2026.
//

#include "ImageNodes.h"
#include <imgui_stdlib.h>
#include <node_based/EditorGraph.h>

namespace Fertilizer {
    SampleImageNode::SampleImageNode(Fertilizer::EditorGraph& graph): Fertilizer::EditorNode(graph, "Sample Image", "sample_image") {
        init();
    }

    SampleImageNode::SampleImageNode(Fertilizer::EditorGraph& graph, const rapidjson::Value& json): Fertilizer::EditorNode(graph, "Sample Image", "sample_image", json) {
        init();
        imagePath = std::string_view{json["extra"]["image_path"].GetString(), json["extra"]["image_path"].GetStringLength()};
    }

    void SampleImageNode::init() {
        newInput("U", Carrot::ExpressionTypes::Float);
        newInput("V", Carrot::ExpressionTypes::Float);
        newOutput("Color", Carrot::ExpressionTypes::Color);
    }

    bool SampleImageNode::renderCenter() {
        bool modified = EditorNode::renderCenter();
        ImGui::SetNextItemWidth(100.0f);
        bool imageModified = ImGui::InputText("Image", &imagePath);
        const ImVec2 size(256, 256);
        const ImTextureID textureID = graph.getImGuiTextures()->requestTextureID(imagePath);
        if (textureID != 0) {
            ImGui::Image(textureID, size);
        } else {
            ImGui::Dummy(size);
        }
        modified |= imageModified;
        if (ImGui::BeginDragDropTarget()) {
            // TODO
            ImGui::EndDragDropTarget();
        }
        return modified;
    }

    std::shared_ptr<Carrot::Expression> SampleImageNode::toExpression(uint32_t outputIndex, std::unordered_set<Carrot::UUID>& activeLinks) const {
        const std::vector<Carrot::Expression::Ptr> inputs = getExpressionsFromInput(activeLinks);
        if (inputs[0] == nullptr) {
            return nullptr;
        }
        if (inputs[1] == nullptr) {
            return nullptr;
        }
        return std::make_shared<Carrot::SampleImageExpression>(imagePath, inputs[0], inputs[1]);
    }

    rapidjson::Value SampleImageNode::serialiseToJSON(rapidjson::MemoryPoolAllocator<>& allocator) const {
        rapidjson::Value obj{rapidjson::kObjectType};
        obj.AddMember("image_path", rapidjson::Value{imagePath.c_str(), static_cast<u32>(imagePath.size()), allocator}, allocator);
        return obj;
    }

    // --- Split Color

    SplitColorNode::SplitColorNode(Fertilizer::EditorGraph& graph): EditorNode(graph, "Split Color RGBA", "split_color_rgba") {
        newInput("Color", Carrot::ExpressionTypes::Color);
        newOutput("R", Carrot::ExpressionTypes::Float);
        newOutput("G", Carrot::ExpressionTypes::Float);
        newOutput("B", Carrot::ExpressionTypes::Float);
        newOutput("A", Carrot::ExpressionTypes::Float);
    }

    SplitColorNode::SplitColorNode(Fertilizer::EditorGraph& graph, const rapidjson::Value& json): EditorNode(graph, "Split Color RGBA", "split_color_rgba", json) {
        newInput("Color", Carrot::ExpressionTypes::Color);
        newOutput("R", Carrot::ExpressionTypes::Float);
        newOutput("G", Carrot::ExpressionTypes::Float);
        newOutput("B", Carrot::ExpressionTypes::Float);
        newOutput("A", Carrot::ExpressionTypes::Float);
    }

    std::shared_ptr<Carrot::Expression> SplitColorNode::toExpression(uint32_t outputIndex, std::unordered_set<Carrot::UUID>& activeLinks) const {
        const std::vector<Carrot::Expression::Ptr> inputs = getExpressionsFromInput(activeLinks);
        if (inputs[0] == nullptr)
            return nullptr;
        return std::make_shared<Carrot::GetVectorComponentExpression>(4, inputs[0], outputIndex);
    }

    // --- Combine Color
    CombineColorNode::CombineColorNode(Fertilizer::EditorGraph& graph): EditorNode(graph, "Combine Color RGBA", "combine_color_rgba") {
        newInput("R", Carrot::ExpressionTypes::Float);
        newInput("G", Carrot::ExpressionTypes::Float);
        newInput("B", Carrot::ExpressionTypes::Float);
        newInput("A", Carrot::ExpressionTypes::Float);
        newOutput("Color", Carrot::ExpressionTypes::Color);
    }

    CombineColorNode::CombineColorNode(Fertilizer::EditorGraph& graph, const rapidjson::Value& json): EditorNode(graph, "Combine Color RGBA", "combine_color_rgba", json) {
        newInput("R", Carrot::ExpressionTypes::Float);
        newInput("G", Carrot::ExpressionTypes::Float);
        newInput("B", Carrot::ExpressionTypes::Float);
        newInput("A", Carrot::ExpressionTypes::Float);
        newOutput("Color", Carrot::ExpressionTypes::Color);
    }

    std::shared_ptr<Carrot::Expression> CombineColorNode::toExpression(uint32_t outputIndex, std::unordered_set<Carrot::UUID>& activeLinks) const {
        std::vector<Carrot::Expression::Ptr> inputs = getExpressionsFromInput(activeLinks);
        for (const auto& pExpr : inputs) {
            if (!pExpr) {
                return nullptr;
            }
        }
        return std::make_shared<Carrot::MakeVectorExpression>(4, inputs);
    }


} // Fertilizer