//
// Created by jglrxavpok on 18/04/2026.
//

#pragma once
#include <core/containers/Vector.hpp>

#include "Expression.h"

namespace Carrot {
    class SampleImageExpression: public Expression {
    public:
        explicit SampleImageExpression(std::string imagePath, Expression::Ptr u, Expression::Ptr v): Expression(), imagePath(std::move(imagePath)), u(u), v(v) {}

        std::any visit(BaseExpressionVisitor& visitor) override;
        ExpressionType getType() override;
        std::string toString() const override;

        const std::string& getImagePath() const;
        Expression::Ptr getU() const;
        Expression::Ptr getV() const;

    private:
        std::string imagePath;
        Expression::Ptr u;
        Expression::Ptr v;
    };

    // Extracts the Nth component of a expression of type vector
    class GetVectorComponentExpression: public Expression {
    public:
        explicit GetVectorComponentExpression(u8 vectorSize, Carrot::Expression::Ptr vectorExpression, u8 componentIndex);

        std::any visit(BaseExpressionVisitor& visitor) override;
        ExpressionType getType() override;
        std::string toString() const override;

        Expression::Ptr getVectorInput() const;
        u8 getComponentIndex() const;

    private:
        Expression::Ptr vectorInput;
        u8 componentIndex = 0;
    };

    class MakeVectorExpression: public Expression {
    public:
        explicit MakeVectorExpression(u8 vectorSize, std::span<Carrot::Expression::Ptr> inputs);

        std::any visit(BaseExpressionVisitor& visitor) override;
        ExpressionType getType() override;
        std::string toString() const override;

        u8 getVectorSize() const;
        std::span<const Expression::Ptr> getInputs() const;

    private:
        u8 vectorSize = 0;
        Carrot::Vector<Expression::Ptr> inputs;
    };
}
