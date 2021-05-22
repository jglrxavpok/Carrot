//
// Created by jglrxavpok on 11/05/2021.
//

#pragma once

#include "Expression.h"
#include <stdexcept>
#include <functional>
#include <vector>

namespace Carrot {
    class ConstantExpression: public Expression {
    private:
        float value = 0.0f;

    public:
        inline explicit ConstantExpression(float v): value(v) {};

        inline float evaluate() const override { return value; };

        inline float getValue() const { return value; };

        void visit(ExpressionVisitor& visitor) override;

        inline std::string toString() const override { return std::to_string(value); };
    };

    class GetVariableExpression: public Expression {
    private:
        std::string varName;
        std::uint32_t subIndex = 0;

    public:
        inline explicit GetVariableExpression(const std::string& name, std::uint32_t subIndex = 0): varName(std::move(name)), subIndex(subIndex) {}

        inline float evaluate() const override {
            return 0.0f; // TODO: pull from some database, or throw?
        };

        inline std::string toString() const override {
            return varName + "(" + std::to_string(subIndex) + ")";
        }

        void visit(ExpressionVisitor& visitor) override;

    public:
        inline const std::string& getVariableName() const { return varName; };
        inline std::uint32_t getSubIndex() const { return subIndex; };
    };

    class SetVariableExpression: public Expression {
    private:
        std::string varName;
        std::shared_ptr<Expression> value;
        std::uint32_t subIndex = 0;

    public:
        inline explicit SetVariableExpression(const std::string& name, std::shared_ptr<Expression> value, std::uint32_t subIndex = 0): varName(std::move(name)), value(value), subIndex(subIndex) {}

        inline float evaluate() const override {
            return value->evaluate();
        };

        inline std::string toString() const override {
            return varName + "(" + std::to_string(subIndex) + ") = " + value->toString();
        }

        void visit(ExpressionVisitor& visitor) override;

    public:
        inline const std::string& getVariableName() const { return varName; };
        inline std::shared_ptr<Expression> getValue() const { return value; };
        inline std::uint32_t getSubIndex() const { return subIndex; };
    };

    class CompoundExpression: public Expression {
    private:
        std::vector<std::shared_ptr<Expression>> value;

    public:
        inline explicit CompoundExpression(const std::vector<std::shared_ptr<Expression>>& value): value(value) {}

        inline float evaluate() const override {
            throw std::runtime_error("Not made for direct evaluation.");
        };

        inline std::string toString() const override {
            std::string contents;
            for(const auto& sub : value) {
                if(sub) {
                    contents += sub->toString() + "; ";
                } else {
                    contents += "<null>; ";
                }
            }
            return "{" + contents + "}";
        }

        void visit(ExpressionVisitor& visitor) override;

    public:
        inline std::vector<std::shared_ptr<Expression>>& getSubExpressions() { return value; };
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

        inline std::shared_ptr<Expression> getOperand1() const { return left; };
        inline std::shared_ptr<Expression> getOperand2() const { return right; };

        void visit(ExpressionVisitor& visitor) override;
    };

    using AddExpression = BinaryOperationExpression<std::plus<float>>;
    using SubExpression = BinaryOperationExpression<std::minus<float>>;
    using MultExpression = BinaryOperationExpression<std::multiplies<float>>;
    using DivExpression = BinaryOperationExpression<std::divides<float>>;

    struct sfmod {
        float operator()(const float& x, const float& y) const {
            return fmod(x, y);
        }
    };
    using ModExpression = BinaryOperationExpression<sfmod>;
}