//
// Created by jglrxavpok on 11/03/2023.
//

#pragma once

#include <core/scripting/csharp/forward.h>
#include <mono/metadata/object-forward.h>
#include <span>

namespace Carrot::Scripting {
    class CSMethod {
    public:
        explicit CSMethod(MonoMethod* method);

        /**
         * Invokes this method on an object instance
         */
        CSObject invoke(const CSObject& instance, std::span<void*> args);

        /**
         * Statically invokes this method
         */
        CSObject staticInvoke(std::span<void*> args);

    private:
        MonoMethod* method = nullptr;

        friend class CSClass;
    };
} // Carrot::Scripting
