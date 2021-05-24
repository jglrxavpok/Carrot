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

    VISIT(Constant)
    VISIT(GetVariable);
    VISIT(SetVariable)
    VISIT(Compound);
    VISIT_T(Add);
    VISIT_T(Sub);
    VISIT_T(Mult);
    VISIT_T(Div);
    VISIT_T(Mod);

    VISIT_T(Less);
    VISIT_T(LessOrEquals);
    VISIT_T(Greater);
    VISIT_T(GreaterOrEquals);
    VISIT_T(Equals);
    VISIT_T(NotEquals);

    VISIT_T(Or);
    VISIT_T(And);
    VISIT_T(Xor);
    VISIT(BoolNegate);

#undef VISIT
#undef VISIT_T

    std::uint32_t ExpressionType::ids = 0;
}