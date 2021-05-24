//
// Created by jglrxavpok on 22/05/2021.
//

#pragma once

#include <stdexcept>
#include "Expressions.h"

namespace Carrot {

    class ExpressionVisitor {
    public:
        inline void visit(std::shared_ptr<Expression> expression) {
            expression->visit(*this);
        };

        inline void visitUnimplemented(Expression& expression) {
            throw std::runtime_error("Unimplemented expression type in visitor");
        }

#define VISIT(Type) \
        inline virtual void visit ## Type(Type ## Expression& expression) { \
            visitUnimplemented(expression); \
        }

        VISIT(Constant);
        VISIT(GetVariable);
        VISIT(SetVariable);
        VISIT(Add);
        VISIT(Sub);
        VISIT(Mult);
        VISIT(Div);
        VISIT(Compound);
        VISIT(Mod);

        VISIT(Less);
        VISIT(LessOrEquals);
        VISIT(Greater);
        VISIT(GreaterOrEquals);
        VISIT(Equals);
        VISIT(NotEquals);

        VISIT(Or);
        VISIT(And);
        VISIT(Xor);
        VISIT(BoolNegate);

#undef VISIT

        virtual ~ExpressionVisitor() = default;
    };
}