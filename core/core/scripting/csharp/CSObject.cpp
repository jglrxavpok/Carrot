//
// Created by jglrxavpok on 11/03/2023.
//

#include "CSObject.h"
#include "core/Macros.h"
#include "mono/metadata/object.h"

namespace Carrot::Scripting {
    CSObject::CSObject(MonoObject* obj): obj(obj) {
        gcHandle = mono_gchandle_new(obj, true);
    }

    CSObject::~CSObject() {
        mono_gchandle_free(gcHandle);
    }

    CSObject::operator bool() const {
        return obj != nullptr;
    }

    CSObject::operator MonoObject*() const {
        return obj;
    }

    MonoObject* CSObject::toMono() const {
        return obj;
    }

    std::string CSObject::toString() const {
        MonoString* asStr = (MonoString*)obj;
        char* str = mono_string_to_utf8(asStr);
        std::string r = str;
        mono_free(str);
        return r;
    }

    void* CSObject::unboxInternal() const {
        if(obj == nullptr) {
            return nullptr;
        }
        return mono_object_unbox(obj);
    }
} // Carrot::Scripting