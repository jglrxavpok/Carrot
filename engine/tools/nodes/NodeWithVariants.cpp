//
// Created by jglrxavpok on 30/05/25.
//

#include "NodeWithVariants.h"

namespace Tools {
    NodeWithVariants::NodeWithVariants(EditorGraph& graph, const std::string& title, const std::string& internalName, u8 variantCount)
    : EditorNode(graph, title, internalName)
    , variantCount(variantCount)
    {}

    NodeWithVariants::NodeWithVariants(EditorGraph& graph, const std::string& title, const std::string& internalName, u8 variantCount, const rapidjson::Value& json)
        : EditorNode(graph, title, internalName, json)
        , variantCount(variantCount)
    {
        if (auto extraMemberIter = json.FindMember("extra"); extraMemberIter != json.MemberEnd()) {
            currentVariant = extraMemberIter->value["current_variant"].GetInt();
        }
    }

    bool NodeWithVariants::renderCenter() {
        return renderCenter(currentVariant);
    }

    bool NodeWithVariants::renderHeaderWidgets() {
        bool modified = renderHeaderWidgets(currentVariant);

        for (u8 variant = 0; variant < variantCount; variant++) {
            ImGui::SameLine();
            const std::string label = Carrot::sprintf("%u", variant);
            if (ImGui::RadioButton(label.c_str(), variant == currentVariant) && currentVariant != variant) {
                u8 oldVariant = currentVariant;
                currentVariant = variant;
                onVariantChanged(oldVariant);
                modified = true;
            }
        }
        return modified;
    }

    bool NodeWithVariants::renderInputPins() {
        return renderInputPins(currentVariant);
    }

    bool NodeWithVariants::renderOutputPins() {
        return renderOutputPins(currentVariant);
    }

    bool NodeWithVariants::renderInputPins(u8 variantIndex) {
        return EditorNode::renderInputPins();
    }

    bool NodeWithVariants::renderOutputPins(u8 variantIndex) {
        return EditorNode::renderInputPins();
    }

    rapidjson::Value NodeWithVariants::serialiseToJSON(rapidjson::Document& doc) const {
        rapidjson::Value result = serialiseToJSON(doc, currentVariant);
        result.AddMember("current_variant", currentVariant, doc.GetAllocator());
        return result;
    }

    std::shared_ptr<Carrot::Expression> NodeWithVariants::toExpression(uint32_t outputIndex, std::unordered_set<Carrot::UUID>& activeLinks) const {
        return toExpression(outputIndex, currentVariant, activeLinks);
    }


} // Tools