//
// Created by jglrxavpok on 31/05/25.
//

#pragma once
#include <node_based/EditorNode.h>
#include <unordered_set>

namespace Fertilizer {

    class CommentNode: public EditorNode {
    public:
        explicit CommentNode(EditorGraph& graph);

        explicit CommentNode(EditorGraph& graph, const rapidjson::Value& json);

    public:
        bool draw() override;
        bool renderHeaderWidgets() override;
        bool renderCenter() override;
        rapidjson::Value serialiseToJSON(rapidjson::MemoryPoolAllocator<>& allocator) const override;
        std::shared_ptr<Carrot::Expression> toExpression(uint32_t outputIndex, std::unordered_set<Carrot::UUID>& activeLinks) const override;

    private:
        std::string commentTitle; // text chosen by user for this comment
        bool editingComment = false; // whether the user is currently editing the text of the comment
        bool showingTextBox = false;
        ImVec2 size{100, 100};
    };

} // Tools