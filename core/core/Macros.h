//
// Created by jglrxavpok on 27/11/2021.
//

#pragma once

//
// Created by jglrxavpok on 19/05/2021.
//

#pragma once

#include <stdexcept>
#include "utils/Assert.h"
#include "utils/Containers.h"
#include <debugbreak.h>
#include <functional>

namespace Carrot::Exceptions {
    class TodoException: public std::exception {
    public:
        TodoException() = default;

        const char * what() const noexcept override {
            return "A feature is not yet implemented";
        }
    };
}

#define TODO throw Carrot::Exceptions::TodoException();
#define DISCARD(x) static_cast<void>((x))

// TODO: use std::unreachable when switching to C++23

// from https://en.cppreference.com/w/cpp/utility/unreachable.html
#if defined(_MSC_VER) && !defined(__clang__)
#define UNREACHABLE __assume(false)
#else
#define UNREACHABLE __builtin_unreachable()
#endif

struct DeferredCleanup {
    std::function<void()> cleanup;

    ~DeferredCleanup() {
        cleanup();
    }
};

#define MERGE(x, y) x##y
#define CLEANUP_HELPER(x,y) DeferredCleanup MERGE(_cleanup, y) {.cleanup = [&]() { x; }}
#define CLEANUP(x) CLEANUP_HELPER(x, __COUNTER__)

#define GetVFS() (*Carrot::IO::Resource::vfsToUse)