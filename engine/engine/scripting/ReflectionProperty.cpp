//
// Created by jglrxavpok on 30/03/2023.
//

#include <engine/scripting/ReflectionProperty.h>

namespace Carrot::Scripting {
    void ReflectionProperty::validate() {
        if(displayName.empty()) {
            displayName = fieldName;
        }

        if(serializationName.empty()) {
            serializationName = fieldName;
        }
    }
}