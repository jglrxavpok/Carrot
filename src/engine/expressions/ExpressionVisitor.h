//
// Created by jglrxavpok on 22/05/2021.
//

#pragma once

#include <stdexcept>
#include <any>
#include "Expressions.h"

namespace Carrot {

#define GENERATE_VISITS(VISIT)               \
    VISIT(Constant);                         \
    VISIT(GetVariable);                      \
    VISIT(SetVariable);                      \
    VISIT(Add);                              \
    VISIT(Sub);                              \
    VISIT(Mult);                             \
    VISIT(Div);                              \
    VISIT(Compound);                         \
    VISIT(Mod);                              \
                                             \
    VISIT(Less);                             \
    VISIT(LessOrEquals);                     \
    VISIT(Greater);                          \
    VISIT(GreaterOrEquals);                  \
    VISIT(Equals);                           \
    VISIT(NotEquals);                        \
                                             \
    VISIT(Or);                               \
    VISIT(And);                              \
    VISIT(Xor);                              \
    VISIT(BoolNegate);                       \
                                             \
    VISIT(Sin);                              \
    VISIT(Cos);                              \
    VISIT(Tan);                              \
    VISIT(Exp);                              \
    VISIT(Abs);                              \
    VISIT(Sqrt);                             \
    VISIT(Log);                              \
                                             \
    VISIT(Placeholder);                      \
    VISIT(Once);                             \
    VISIT(Prefixed);                         \


    class BaseExpressionVisitor {
    public:

        inline std::any _visitUnimplemented(Expression& expression) {
            throw std::runtime_error("Unimplemented expression type in visitor");
        }

#define VISIT_ACTION(Type) \
        inline virtual std::any _visit ## Type(Type ## Expression& expression) {            \
            return _visitUnimplemented(expression);                                         \
        }                                                                                   \
                                                                                            \

        GENERATE_VISITS(VISIT_ACTION)
#undef VISIT_ACTION


        virtual ~BaseExpressionVisitor() = default;
    };

    template<typename ReturnType = void>
    class IExpressionVisitor: public BaseExpressionVisitor {
    public:
        inline ReturnType visit(std::shared_ptr<Expression> expression) {
            if constexpr(std::is_void_v<ReturnType>) {
                expression->visit(*this);
            } else {
                return std::any_cast<ReturnType>(expression->visit(*this));
            }
        };

        inline ReturnType visitUnimplemented(Expression& expression) {
            if constexpr(std::is_void_v<ReturnType>) {
                _visitUnimplemented(expression);
            } else {
                return std::any_cast<ReturnType>(_visitUnimplemented(expression));
            }
        }

#define VISIT_ACTION(Type) \
        inline virtual ReturnType visit ## Type(Type ## Expression& expression) {               \
            if constexpr(std::is_void_v<ReturnType>) {                                          \
                visitUnimplemented(expression);                                                 \
            } else {                                                                            \
                return visitUnimplemented(expression);                                          \
            }                                                                                   \
        }                                                                                       \
                                                                                                \
        inline std::any _visit ## Type(Type ## Expression& expression) override {               \
            if constexpr(std::is_void_v<ReturnType>) {                                          \
                visit ## Type(expression);                                                      \
                return std::make_any<int>(0);                                                   \
            } else {                                                                            \
                return visit ## Type(expression);                                               \
            }                                                                                   \
        }                                                                                       \

        GENERATE_VISITS(VISIT_ACTION)
#undef VISIT_ACTION
    };

    using ExpressionVisitor = IExpressionVisitor<void>;
}

#undef GENERATE_VISITS