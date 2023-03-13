//
// Created by jglrxavpok on 11/03/2023.
//

#include "CSField.h"
#include "mono/metadata/object.h"
#include <core/Macros.h>
#include <core/scripting/csharp/CSClass.h>
#include <core/scripting/csharp/CSObject.h>

namespace Carrot::Scripting {

    CSField::CSField(CSClass& parent, MonoDomain* appDomain, MonoClassField* field): parent(parent), appDomain(appDomain), field(field) {

    }

    CSObject CSField::get(const CSObject& instance) const {
        return CSObject(mono_field_get_value_object(appDomain, field, instance));
    }

    void CSField::set(const CSObject& instance, const CSObject& value) {
        mono_field_set_value(instance, field, value);
    }

} // Carrot::Scripting