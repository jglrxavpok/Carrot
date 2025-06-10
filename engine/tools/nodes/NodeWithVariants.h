//
// Created by jglrxavpok on 30/05/25.
//

#pragma once
#include <tools/EditorNode.h>

namespace Tools {

    class NodeWithVariants: public EditorNode {
    public:
        explicit NodeWithVariants(EditorGraph& graph, const std::string& title, const std::string& internalName, u8 variantCount);

        /// Deserialisation
        explicit NodeWithVariants(EditorGraph& graph, const std::string& title, const std::string& internalName, u8 variantCount, const rapidjson::Value& json);

    public: // redirect Node base methods with a variant index
        virtual std::shared_ptr<Carrot::Expression> toExpression(uint32_t outputIndex, u8 variantIndex, std::unordered_set<Carrot::UUID>& activeLinks) const = 0;
        virtual bool renderHeaderWidgets(u8 variantIndex) = 0;
        virtual bool renderCenter(u8 variantIndex) = 0;
        virtual rapidjson::Value serialiseToJSON(rapidjson::MemoryPoolAllocator<>& allocator, u8 variantIndex) const = 0;
        virtual void onVariantChanged(u8 oldVariant) {};
        virtual bool renderInputPins(u8 variantIndex);
        virtual bool renderOutputPins(u8 variantIndex);

    public:
        std::shared_ptr<Carrot::Expression> toExpression(uint32_t outputIndex, std::unordered_set<Carrot::UUID>& activeLinks) const final;
        bool renderHeaderWidgets() final;
        bool renderCenter() final;
        rapidjson::Value serialiseToJSON(rapidjson::MemoryPoolAllocator<>& allocator) const final;
        bool renderInputPins() final;
        bool renderOutputPins() final;

    public:
        const u8 variantCount = 0;
    protected:
        u8 currentVariant = 0;
    };

} // Tools