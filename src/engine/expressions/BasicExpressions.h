//
// Created by jglrxavpok on 11/05/2021.
//

#pragma once

#include "Expression.h"
#include <stdexcept>
#include <functional>
#include <vector>
#include <cassert>

namespace Carrot {
    class ConstantExpression: public Expression {
    private:
        union ConstantValue {
            float asFloat;
            bool asBool;
            int asInt;
        } value;

        ExpressionType type = ExpressionTypes::Float;

    public:
        inline explicit ConstantExpression(float v) {
            value.asFloat = v;
            type = ExpressionTypes::Float;
        };

        inline explicit ConstantExpression(int v) {
            value.asInt = v;
            type = ExpressionTypes::Int;
        };

        inline explicit ConstantExpression(bool v) {
            value.asBool = v;
            type = ExpressionTypes::Bool;
        };

        ExpressionType getType() override {
            return type;
        };

        inline ConstantValue getValue() const { return value; };

        std::any visit(BaseExpressionVisitor& visitor) override;

        inline std::string toString() const override {
            if(type == ExpressionTypes::Float) {
                return std::to_string(value.asFloat);
            }
            if(type == ExpressionTypes::Int) {
                return std::to_string(value.asInt);
            }
            if(type == ExpressionTypes::Bool) {
                return std::to_string(value.asBool);
            }
            return "UNKNOWN TYPE";
        };
    };

    class GetVariableExpression: public Expression {
    private:
        std::string varName;
        std::uint32_t subIndex = 0;
        ExpressionType variableType;

    public:
        inline explicit GetVariableExpression(ExpressionType variableType, const std::string& name, std::uint32_t subIndex = 0): variableType(variableType), varName(std::move(name)), subIndex(subIndex) {}

        inline std::string toString() const override {
            return varName + "(" + std::to_string(subIndex) + ")";
        }

        std::any visit(BaseExpressionVisitor& visitor) override;

    public:
        ExpressionType getType() override { return variableType; };

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

        inline std::string toString() const override {
            return varName + "(" + std::to_string(subIndex) + ") = " + value->toString();
        }

        std::any visit(BaseExpressionVisitor& visitor) override;

    public:
        ExpressionType getType() override { return ExpressionTypes::Void; };

        inline const std::string& getVariableName() const { return varName; };
        inline std::shared_ptr<Expression> getValue() const { return value; };
        inline std::uint32_t getSubIndex() const { return subIndex; };
    };

    class CompoundExpression: public Expression {
    private:
        std::vector<std::shared_ptr<Expression>> value;

    public:
        inline explicit CompoundExpression(const std::vector<std::shared_ptr<Expression>>& value): value(value) {}

        ExpressionType getType() override { return ExpressionTypes::Void; };

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

        std::any visit(BaseExpressionVisitor& visitor) override;

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
        inline explicit BinaryOperationExpression(std::shared_ptr<Expression> left, std::shared_ptr<Expression> right): Expression(), left(left), right(right) {
            assert(left->getType() == right->getType());
        };

        ExpressionType getType() override { return left->getType(); };

        inline std::string toString() const override {
            return left->toString() + " " + typeid(Op).name() + " " + right->toString();
        }

        inline std::shared_ptr<Expression> getOperand1() const { return left; };
        inline std::shared_ptr<Expression> getOperand2() const { return right; };

        std::any visit(BaseExpressionVisitor& visitor) override;
    };

    using AddExpression = BinaryOperationExpression<std::plus<float>>;
    using SubExpression = BinaryOperationExpression<std::minus<float>>;
    using MultExpression = BinaryOperationExpression<std::multiplies<float>>;
    using DivExpression = BinaryOperationExpression<std::divides<float>>;

    struct fmodstruct {
        float operator()(const float& x, const float& y) const {
            return fmod(x, y);
        }
    };

    using ModExpression = BinaryOperationExpression<fmodstruct>;

    class PlaceholderExpression: public Expression {
    public:
        inline explicit PlaceholderExpression(std::shared_ptr<void> data, Carrot::ExpressionType type): Expression(), data(data), type(type) {}

        ExpressionType getType() override { return type; }

        inline std::string toString() const override {
            return "<PLACEHOLDER>";
        }

        inline std::shared_ptr<void> getData() const { return data; }

        std::any visit(BaseExpressionVisitor& visitor) override;

    private:
        std::shared_ptr<void> data;
        Carrot::ExpressionType type;
    };
}