//
// Created by jglrxavpok on 22/05/2021.
//

#include "Expressions.h"
#include "ExpressionVisitor.h"

namespace Carrot {
#define VISIT(Type) \
    void Type ## Expression::visit(ExpressionVisitor& visitor) { visitor.visit ## Type(*this); }

#define VISIT_T(Type) \
    template<> VISIT(Type)

    VISIT(Constant);
    VISIT(GetVariable);
    VISIT(SetVariable);
    VISIT(Compound);
    VISIT_T(Add);
    VISIT_T(Sub);
    VISIT_T(Mult);
    VISIT_T(Div);
    VISIT_T(Mod);

#undef VISIT
#undef VISIT_T
}