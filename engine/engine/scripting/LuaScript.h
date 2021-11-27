//
// Created by jglrxavpok on 12/09/2021.
//

#pragma once

#include "bindings/all.hpp"
#include <core/io/Resource.h>

namespace Carrot::Lua {
    class Script: public sol::state {
    public:
        /// Creates a script object from a resource (file or in-memory)
        explicit Script(const Carrot::IO::Resource& script);

        /// Creates a script object directly from raw text
        Script(const std::string& text);

        /// Creates a script object directly from raw text
        Script(const char* text);

        template<typename ReturnType, typename... Args>
        ReturnType callWithThrow(std::string_view functionName, Args&&... args) {
            sol::protected_function protectedVersion = (*this)[functionName];
            auto result = protectedVersion(std::forward<Args>(args)...);
            if (result.valid()) {
                if constexpr(!std::is_void_v<ReturnType>) {
                    return result;
                }
            } else {
                // An error has occured
                sol::error err = result;
                std::string what = err.what();
                throw std::runtime_error("Lua error: " + what);
            }
        }
    };
}
