//
// Created by jglrxavpok on 19/05/2021.
//

#pragma once
#include <stdexcept>

namespace Carrot::Exceptions {
    class TodoException: public std::exception {
    public:
        TodoException() = default;

        const char * what() const override {
            return "A feature is not yet implemented";
        }
    };
}

#define TODO throw Carrot::Exceptions::TodoException();
#define DISCARD(x) static_cast<void>((x))