//
// Created by jglrxavpok on 11/03/2023.
//

#pragma once

#include <cstdint>
#include <string>
#include <core/scripting/csharp/forward.h>
#include <mono/metadata/object-forward.h>

namespace Carrot::Scripting {

    /**
     * Wrapper around a C# sharp object. Intended to help with hot-reloading.
     */
    class CSObject {
    public:
        explicit CSObject(MonoObject* obj = nullptr);
        ~CSObject();

        /**
         * Converts to true iif this object is still valid (ie not removed because of hot-reloading)
         */
        explicit operator bool() const;

        operator MonoObject*() const;

        MonoObject* toMono() const;

        /**
         * Assumes this object is a "string" (C# type) and **copies** its content to a std::string.
         */
        std::string toString() const;

        /**
         * Unbox this object to the given type
         * @tparam ToType
         * @return
         */
        template<typename ToType>
        ToType unbox() const {
            return *((ToType*)unboxInternal());
        }

    private:
        void* unboxInternal() const;

    private:
        mutable MonoObject* obj = nullptr;
        std::uint32_t gcHandle = 0;
    };

} // Carrot::Scripting
