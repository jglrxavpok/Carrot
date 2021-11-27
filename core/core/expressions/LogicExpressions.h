//
// Created by jglrxavpok on 24/05/2021.
//

#pragma once

#include "BasicExpressions.h"

namespace Carrot {
    template<typename Op>
    class BinaryComparisonExpression: public Expression {
    private:
        inline static Op op{};
        std::shared_ptr<Expression> left;
        std::shared_ptr<Expression> right;

    public:
        inline explicit BinaryComparisonExpression(std::shared_ptr<Expression> left, std::shared_ptr<Expression> right): Expression(), left(left), right(right) {
            assert(left->getType() == right->getType());
        };

        ExpressionType getType() override { return ExpressionTypes::Bool; };

        inline std::string toString() const override {
            return left->toString() + " " + typeid(Op).name() + " " + right->toString();
        }

        inline std::shared_ptr<Expression> getOperand1() const { return left; };
        inline std::shared_ptr<Expression> getOperand2() const { return right; };

        std::any visit(BaseExpressionVisitor& visitor) override;
    };

    using LessExpression = BinaryComparisonExpression<std::less<float>>;
    using LessOrEqualsExpression = BinaryComparisonExpression<std::less_equal<float>>;
    using GreaterExpression = BinaryComparisonExpression<std::greater<float>>;
    using GreaterOrEqualsExpression = BinaryComparisonExpression<std::greater_equal<float>>;
    using EqualsExpression = BinaryComparisonExpression<std::equal_to<float>>;
    using NotEqualsExpression = BinaryComparisonExpression<std::not_equal_to<float>>;

    template<typename Op>
    class LogicOperatorExpression: public Expression {
    private:
        inline static Op op{};
        std::shared_ptr<Expression> left;
        std::shared_ptr<Expression> right;

    public:
        inline explicit LogicOperatorExpression(std::shared_ptr<Expression> left, std::shared_ptr<Expression> right): Expression(), left(left), right(right) {
            assert(left->getType() == Carrot::ExpressionTypes::Bool);
            assert(right->getType() == Carrot::ExpressionTypes::Bool);
        };

        ExpressionType getType() override { return ExpressionTypes::Bool; };

        inline std::string toString() const override {
            return left->toString() + " " + typeid(Op).name() + " " + right->toString();
        }

        inline std::shared_ptr<Expression> getOperand1() const { return left; };
        inline std::shared_ptr<Expression> getOperand2() const { return right; };

        std::any visit(BaseExpressionVisitor& visitor) override;
    };

    struct sbxor {
        bool operator()(const bool& a, const bool& b) const {
            return a ^ b;
        }
    };

    using OrExpression = LogicOperatorExpression<std::logical_or<bool>>;
    using XorExpression = LogicOperatorExpression<sbxor>;
    using AndExpression = LogicOperatorExpression<std::logical_and<bool>>;

    class BoolNegateExpression: public Expression {
    private:
        std::shared_ptr<Expression> operand;

    public:
        inline explicit BoolNegateExpression(std::shared_ptr<Expression> operand): Expression(), operand(operand) {
            assert(operand->getType() == Carrot::ExpressionTypes::Bool);
        };

        ExpressionType getType() override { return ExpressionTypes::Bool; };

        inline std::string toString() const override {
            return "! " + operand->toString();
        }

        inline std::shared_ptr<Expression> getOperand() const { return operand; };

        std::any visit(BaseExpressionVisitor& visitor) override;
    };
}