//
// Created by jglrxavpok on 27/11/2021.
//

#pragma once

//
// Created by jglrxavpok on 19/05/2021.
//

#pragma once

#include <stdexcept>
#include <core/Macros.h>
#include "utils/Assert.h"
#include "utils/Containers.h"

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
