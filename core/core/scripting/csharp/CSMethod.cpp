//
// Created by jglrxavpok on 11/03/2023.
//

#include "CSMethod.h"
#include "core/utils/Assert.h"
#include <core/scripting/csharp/CSObject.h>
#include <mono/metadata/object.h>

namespace Carrot::Scripting {
    CSMethod::CSMethod(MonoMethod *method): method(method) {

    }

    CSObject CSMethod::invoke(const CSObject& obj, std::span<void *> args) {
        MonoObject* exception = nullptr;
        auto result = CSObject(mono_runtime_invoke(method, obj, args.data(), &exception));

        // TODO: throw C++ exception
        if(exception) {
            mono_print_unhandled_exception(exception);
            verify(false, "C# exception");
        }
        return result;
    }

    CSObject CSMethod::staticInvoke(std::span<void *> args) {
        MonoObject* exception = nullptr;
        auto result = CSObject(mono_runtime_invoke(method, nullptr, args.data(), &exception));

        // TODO: throw C++ exception
        if(exception) {
            mono_print_unhandled_exception(exception);
            verify(false, "C# exception");
        }
        return result;
    }
} // Carrot::Scripting