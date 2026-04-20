//
// Created by jglrxavpok on 18/04/2026.
//

#pragma once
#include <node_based/EditorNode.h>

namespace Fertilizer {
    class SampleImageNode: public Fertilizer::EditorNode {
    public:
        explicit SampleImageNode(Fertilizer::EditorGraph& graph);
        explicit SampleImageNode(Fertilizer::EditorGraph& graph, const rapidjson::Value& json);

    public:
        rapidjson::Value serialiseToJSON(rapidjson::MemoryPoolAllocator<>& allocator) const override;
        std::shared_ptr<Carrot::Expression> toExpression(uint32_t outputIndex, std::unordered_set<Carrot::UUID>& activeLinks) const override;

        bool renderCenter() override;

    private:
        std::string imagePath;

        void init();
    };

    class SplitColorNode: public Fertilizer::EditorNode {
    public:
        explicit SplitColorNode(Fertilizer::EditorGraph& graph);
        explicit SplitColorNode(Fertilizer::EditorGraph& graph, const rapidjson::Value& json);

    public:
        std::shared_ptr<Carrot::Expression> toExpression(uint32_t outputIndex, std::unordered_set<Carrot::UUID>& activeLinks) const override;
    };

    class CombineColorNode: public Fertilizer::EditorNode {
    public:
        explicit CombineColorNode(Fertilizer::EditorGraph& graph);
        explicit CombineColorNode(Fertilizer::EditorGraph& graph, const rapidjson::Value& json);

    public:
        std::shared_ptr<Carrot::Expression> toExpression(uint32_t outputIndex, std::unordered_set<Carrot::UUID>& activeLinks) const override;
    };
} // Fertilizer