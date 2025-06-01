//
// Created by jglrxavpok on 31/05/25.
//

#include "CommentNode.h"

#include <core/utils/ImGuiUtils.hpp>
#include <tools/EditorGraph.h>

namespace Tools {
    CommentNode::CommentNode(EditorGraph& graph): EditorNode(graph, "Comment", "comment") {}
    CommentNode::CommentNode(EditorGraph& graph, const rapidjson::Value& json): EditorNode(graph, "Comment", "comment", json) {
        commentTitle = json["extra"]["comment"].GetString();
        const glm::vec2 readSize = Carrot::JSON::read<2, float>(json["extra"]["size"]);
        size.x = readSize.x;
        size.y = readSize.y;
    }

    bool CommentNode::draw() {
        // Heavily based on https://github.com/thedmd/imgui-node-editor/blob/master/examples/blueprints-example/utilities/builders.cpp
        const u32 groupID = graph.getEditorID(getID());
        const float commentAlpha = 0.75f;

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, commentAlpha);
        ed::BeginNode(groupID);
        ImGui::PushID(groupID);
        ImGui::BeginVertical("content");
        ImGui::BeginHorizontal("horizontal");
        ImGui::Spring(1);

        const float inputWidth = std::max(ImGui::CalcTextSize(commentTitle.c_str()).x + ImGui::GetStyle().WindowPadding.x*2, 100.0f);;
        if (!editingComment) {
            if (ImGui::Button(commentTitle.c_str(), ImVec2(inputWidth, 0))) {
                editingComment = true;
            }
            showingTextBox = false;
        } else {
            ImGui::SetNextItemWidth(inputWidth);
            if (ImGui::InputText("##comment text", commentTitle, ImGuiInputTextFlags_EnterReturnsTrue)) {
                editingComment = false;
            }
            if (!showingTextBox) {
                ImGui::SetKeyboardFocusHere(-1);
            } else if (!ImGui::IsItemFocused()) {
                editingComment = false;
            }
            showingTextBox = true;
        }

        ImGui::Spring(1);
        ImGui::EndHorizontal();
        ed::Group(size);
        ImGui::EndVertical();
        ImGui::PopID();
        ed::EndNode();
        ImGui::PopStyleVar();

        if (ed::BeginGroupHint(groupID))
        {
            auto bgAlpha = static_cast<int>(ImGui::GetStyle().Alpha * 255);

            auto min = ed::GetGroupMin();

            ImGui::SetCursorScreenPos(min - ImVec2(-8, ImGui::GetTextLineHeightWithSpacing() + 4));
            ImGui::BeginGroup();
            ImGui::TextUnformatted(commentTitle.c_str());
            ImGui::EndGroup();

            auto drawList = ed::GetHintBackgroundDrawList();

            auto ImGui_GetItemRect = []
            {
                return ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
            };

            auto ImRect_Expanded = [](const ImRect& rect, float x, float y)
            {
                auto result = rect;
                result.Min.x -= x;
                result.Min.y -= y;
                result.Max.x += x;
                result.Max.y += y;
                return result;
            };

            auto hintBounds      = ImGui_GetItemRect();
            auto hintFrameBounds = ImRect_Expanded(hintBounds, 8, 4);

            drawList->AddRectFilled(
                hintFrameBounds.GetTL(),
                hintFrameBounds.GetBR(),
                IM_COL32(255, 255, 255, 64 * bgAlpha / 255), 4.0f);

            drawList->AddRect(
                hintFrameBounds.GetTL(),
                hintFrameBounds.GetBR(),
                IM_COL32(255, 255, 255, 128 * bgAlpha / 255), 4.0f);
        }
        ed::EndGroupHint();

        size = ed::GetNodeSize(groupID);
        handlePosition();
        return false;
    }

    rapidjson::Value CommentNode::serialiseToJSON(rapidjson::Document& doc) const {
        rapidjson::Value r{rapidjson::kObjectType};
        r.AddMember("comment", rapidjson::Value{commentTitle, doc.GetAllocator()}, doc.GetAllocator());
        r.AddMember("size", Carrot::JSON::write(glm::vec2{size.x, size.y}, doc.GetAllocator()), doc.GetAllocator());
        return r;
    }

    std::shared_ptr<Carrot::Expression> CommentNode::toExpression(uint32_t outputIndex, std::unordered_set<Carrot::UUID>& activeLinks) const {
        return nullptr;
    }

    bool CommentNode::renderCenter() {
        UNREACHABLE; // draw() overridden for custom draw, no need for this method
    }

    bool CommentNode::renderHeaderWidgets() {
        UNREACHABLE; // draw() overridden for custom draw, no need for this method
    }


} // Tools