//
// Created by jglrxavpok on 11/05/2021.
//

#pragma once

#include <memory>
#include <type_traits>
#include <string>
#include "ExpressionType.h"

namespace Carrot {
    class ExpressionVisitor;

    class Expression: public std::enable_shared_from_this<Expression> {
    public:
        Expression() = default;
        virtual ~Expression() = default;

    public:
        virtual void visit(ExpressionVisitor& visitor) = 0;

    public:
        virtual ExpressionType getType() = 0;

        virtual std::string toString() const = 0;
    };
}
