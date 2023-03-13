//
// Created by jglrxavpok on 11/03/2023.
//

#include "CSProperty.h"
#include "mono/metadata/object.h"
#include <core/Macros.h>
#include <core/scripting/csharp/CSClass.h>
#include <core/scripting/csharp/CSMethod.h>
#include <core/scripting/csharp/CSObject.h>

namespace Carrot::Scripting {

    CSProperty::CSProperty(CSClass& parent, MonoProperty* property): parent(parent), property(property) {
        getter = mono_property_get_get_method(property);
        setter = mono_property_get_set_method(property);
    }

    CSObject CSProperty::get(const CSObject& instance) const {
        return CSObject(mono_runtime_invoke(getter, instance, nullptr, nullptr));
    }

    void CSProperty::set(const CSObject& instance, const CSObject& value) {
        MonoObject* obj = (MonoObject*)value;
        void* args[1] = {
                &obj
        };
        mono_runtime_invoke(setter, instance, args, nullptr);
    }

} // Carrot::Scripting