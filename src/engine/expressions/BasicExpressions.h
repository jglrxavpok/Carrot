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

        inline std::string toString() const override { return std::to_string(value); };
    };

    class GetVariableExpression: public Expression {
    private:
        std::string varName;

    public:
        inline explicit GetVariableExpression(const std::string& name): varName(std::move(name)) {}

        inline float evaluate() const override {
            return 0.0f; // TODO: pull from some database, or throw?
        };

        inline std::string toString() const override {
            return varName;
        }

    public:
        inline const std::string& getVariableName() const { return varName; };
    };

    class SetVariableExpression: public Expression {
    private:
        std::string varName;
        std::shared_ptr<Expression> value;

    public:
        inline explicit SetVariableExpression(const std::string& name, std::shared_ptr<Expression> value): varName(std::move(name)), value(value) {}

        inline float evaluate() const override {
            return value->evaluate();
        };

        inline std::string toString() const override {
            return varName + " = " + value->toString();
        }

    public:
        inline const std::string& getVariableName() const { return varName; };
        inline std::shared_ptr<Expression> getValue() const { return value; };
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
    };

    using AddExpression = BinaryOperationExpression<std::plus<float>>;
    using SubExpression = BinaryOperationExpression<std::minus<float>>;
    using MultExpression = BinaryOperationExpression<std::multiplies<float>>;
    using DivExpression = BinaryOperationExpression<std::divides<float>>;
}