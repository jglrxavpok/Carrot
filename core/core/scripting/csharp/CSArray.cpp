//
// Created by jglrxavpok on 11/03/2023.
//

#include "CSArray.h"
#include "mono/metadata/object.h"
#include <core/Macros.h>
#include <core/scripting/csharp/CSClass.h>
#include <core/scripting/csharp/CSObject.h>

namespace Carrot::Scripting {

    CSArray::CSArray(CSClass& parent, MonoDomain* appDomain, MonoArray* array): parent(parent), appDomain(appDomain), array(array) {
        gcHandle = mono_gchandle_new((MonoObject*)array, true);
    }

    CSArray::~CSArray() {
        mono_gchandle_free(gcHandle);
    }

    void CSArray::set(std::size_t index, const CSObject& value) {
        mono_array_setref(array, index, value);
    }

    CSObject CSArray::get(std::size_t index) {
        return CSObject(mono_array_get(array, MonoObject*, index));
    }

    MonoArray* CSArray::toMono() {
        return array;
    }

} // Carrot::Scripting