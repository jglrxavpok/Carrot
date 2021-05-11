//
// Created by jglrxavpok on 11/05/2021.
//

#pragma once

#include "Expression.h"

namespace Carrot {
    class ConstantExpression: public Expression {
    private:
        float value = 0.0f;

    public:
        inline explicit ConstantExpression(float v): value(v) {};

        inline float evaluate() const override { return value; };
    };

    template<typename Op>
    class BinaryOperationExpression: public Expression {
    private:
        static Op op{};
        std::shared_ptr<Expression> left;
        std::shared_ptr<Expression> right;

    public:
        inline explicit BinaryOperationExpression(std::shared_ptr<Expression> left, std::shared_ptr<Expression> right): Expression(), left(left), right(right) {};

        inline float evaluate() const override {
            return op(left->evaluate(), right->evaluate());
        }
    };

    using AddExpression = BinaryOperationExpression<std::plus<float>>;
    using SubExpression = BinaryOperationExpression<std::minus<float>>;
    using MultExpression = BinaryOperationExpression<std::multiplies<float>>;
    using DivExpression = BinaryOperationExpression<std::divides<float>>;
}