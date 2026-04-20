//
// Created by jglrxavpok on 18/04/2026.
//

#include <core/expressions/ImageExpressions.h>
#include <core/utils/stringmanip.h>

#include "ExpressionVisitor.h"

namespace Carrot {
    ExpressionType SampleImageExpression::getType() {
        return ExpressionTypes::Color;
    }

    std::string SampleImageExpression::toString() const {
        return "SampleImage(" + imagePath + ", uv=(" + u->toString() + "; " + v->toString() + ")";
    }

    const std::string& SampleImageExpression::getImagePath() const {
        return imagePath;
    }

    Expression::Ptr SampleImageExpression::getU() const {
        return u;
    }

    Expression::Ptr SampleImageExpression::getV() const {
        return v;
    }

    GetVectorComponentExpression::GetVectorComponentExpression(u8 vectorSize, Carrot::Expression::Ptr vectorExpression, u8 componentIndex): Expression(), vectorInput(vectorExpression), componentIndex(componentIndex) {
        verify(vectorExpression->getType() == ExpressionTypes::Color, "Input is not a vector"); // Only vector type supported for now
        verify(vectorSize == 4, Carrot::sprintf("Input is not a vector of expected size (4), got %d", vectorSize)); // Only vector type supported for now
    }

    ExpressionType GetVectorComponentExpression::getType() {
        return ExpressionTypes::Float;
    }

    std::string GetVectorComponentExpression::toString() const {
        return "GetVectorComponent(" + vectorInput->toString() + "[" + std::to_string(componentIndex) + "])";
    }

    Expression::Ptr GetVectorComponentExpression::getVectorInput() const {
        return vectorInput;
    }

    u8 GetVectorComponentExpression::getComponentIndex() const {
        return componentIndex;
    }

    MakeVectorExpression::MakeVectorExpression(u8 vectorSize, std::span<Carrot::Expression::Ptr> inputs): Expression(), inputs(inputs), vectorSize(vectorSize) {
        verify(vectorSize == 4, "Invalid vector size"); // only size supported for now
        verify(inputs.size() == vectorSize, Carrot::sprintf("Wrong input count, expected %d, got %d", inputs.size(), vectorSize));
        for (const auto& pExpression : inputs) {
            verify(pExpression->getType() == ExpressionTypes::Float, "Input of wrong type, expected all floats");
        }
    }

    ExpressionType MakeVectorExpression::getType() {
        return ExpressionTypes::Color;
    }

    std::string MakeVectorExpression::toString() const {
        std::string result = "MakeVector(" + std::to_string(vectorSize);
        for (const auto& pExpr : inputs) {
            result += ", ";
            result += pExpr->toString();
        }
        result += ")";
        return result;
    }

    std::span<const Expression::Ptr> MakeVectorExpression::getInputs() const {
        return inputs;
    }

    u8 MakeVectorExpression::getVectorSize() const {
        return vectorSize;
    }

}
