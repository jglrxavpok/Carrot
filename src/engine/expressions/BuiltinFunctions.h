//
// Created by jglrxavpok on 26/05/2021.
//

#pragma once

namespace Carrot {
    template<const char* name>
    class BuiltinFloatFunction: public Expression {
    private:
        std::shared_ptr<Expression> operand;

    public:
        explicit BuiltinFloatFunction(std::shared_ptr<Expression> operand): Expression(), operand(operand) {
            assert(operand->getType() == Carrot::ExpressionTypes::Float);
        };

        ExpressionType getType() override { return Carrot::ExpressionTypes::Float; };

        std::string toString() const override {
            return std::string(name) + "(" + operand->toString() + ")";
        }

        std::shared_ptr<Expression> getOperand() const { return operand; };

        void visit(ExpressionVisitor& visitor) override;
    };

    namespace __BuiltingFunctionNames {
        inline const char sin[] = "sin";
        inline const char cos[] = "cos";
        inline const char tan[] = "tan";
        inline const char exp[] = "exp";
        inline const char abs[] = "abs";
        inline const char sqrt[] = "sqrt";
        inline const char log[] = "log";
    };

    using SinExpression = BuiltinFloatFunction<__BuiltingFunctionNames::sin>;
    using CosExpression = BuiltinFloatFunction<__BuiltingFunctionNames::cos>;
    using TanExpression = BuiltinFloatFunction<__BuiltingFunctionNames::tan>;
    using ExpExpression = BuiltinFloatFunction<__BuiltingFunctionNames::exp>;
    using AbsExpression = BuiltinFloatFunction<__BuiltingFunctionNames::abs>;
    using SqrtExpression = BuiltinFloatFunction<__BuiltingFunctionNames::sqrt>;
    using LogExpression = BuiltinFloatFunction<__BuiltingFunctionNames::log>;
}