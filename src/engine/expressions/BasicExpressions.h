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

        inline std::string toString() const override { return std::to_string(value); };
    };

    class SetVariableExpression: public Expression {
    private:
        std::string varName;
        std::shared_ptr<Expression> value;

    public:
        inline explicit SetVariableExpression(const std::string& name, std::shared_ptr<Expression> value): varName(std::move(varName)), value(value) {}

        inline float evaluate() const override {
            return value->evaluate();
        };

        inline std::string toString() const override {
            return varName + " = " + value->toString();
        }
    };

    template<typename Op>
    class BinaryOperationExpression: public Expression {
    private:
        inline static Op op{};
        std::shared_ptr<Expression> left;
        std::shared_ptr<Expression> right;

    public:
        inline explicit BinaryOperationExpression(std::shared_ptr<Expression> left, std::shared_ptr<Expression> right): Expression(), left(left), right(right) {};

        inline float evaluate() const override {
            return op(left->evaluate(), right->evaluate());
        }

        inline std::string toString() const override {
            return left->toString() + " " + typeid(Op).name() + " " + right->toString();
        }
    };

    using AddExpression = BinaryOperationExpression<std::plus<float>>;
    using SubExpression = BinaryOperationExpression<std::minus<float>>;
    using MultExpression = BinaryOperationExpression<std::multiplies<float>>;
    using DivExpression = BinaryOperationExpression<std::divides<float>>;
}