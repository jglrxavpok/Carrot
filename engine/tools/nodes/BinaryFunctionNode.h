//
// Created by jglrxavpok on 30/05/25.
//

#pragma once
#include <tools/nodes/NodeWithVariants.h>

namespace Tools {

    /// Represents a node which takes two inputs, and outputs a single output.
    /// All inputs and the output have the same type
    /// This node has 3 variants: 2 input pins, 1 input pin + constant, constant + 1 input pin
    class BinaryFunctionNode: public NodeWithVariants {
    public:
        BinaryFunctionNode(EditorGraph& graph, const std::string& title, const std::string& internalName, const Carrot::ExpressionType& outputType);
        BinaryFunctionNode(EditorGraph& graph, const std::string& title, const std::string& internalName, const Carrot::ExpressionType& outputType, const rapidjson::Value& json);

        /// Generates the expression based on the links of this node, and based on the current variant
        virtual std::shared_ptr<Carrot::Expression> toExpression(std::shared_ptr<Carrot::Expression> left, std::shared_ptr<Carrot::Expression> right) const = 0;

    public:
        std::shared_ptr<Carrot::Expression> toExpression(uint32_t outputIndex, u8 variantIndex, std::unordered_set<Carrot::UUID>& activeLinks) const override;
        bool renderHeaderWidgets(u8 variantIndex) override;
        bool renderCenter(u8 variantIndex) override;
        rapidjson::Value serialiseToJSON(rapidjson::MemoryPoolAllocator<>& doc, u8 variantIndex) const override;
        void onVariantChanged(u8 oldVariant) override;
        bool renderInputPins(u8 variantIndex) override;
        bool renderOutputPins(u8 variantIndex) override;

    private:
        void changeInputCount(u8 inputCount);

    private:
        float constant = 0.0f; // constant used in variants with a single input
    };

} // Tools