//
// Created by jglrxavpok on 11/05/2021.
//

#pragma once

#include <memory>
#include <type_traits>

namespace Carrot {
    class Expression: public std::enable_shared_from_this<Expression> {
    public:
        Expression() = default;
        virtual ~Expression() = default;

    public:
        virtual float evaluate() const = 0;
    };
}
