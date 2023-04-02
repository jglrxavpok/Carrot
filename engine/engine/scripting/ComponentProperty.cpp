//
// Created by jglrxavpok on 30/03/2023.
//

#include <engine/scripting/ComponentProperty.h>

namespace Carrot::Scripting {
    void ComponentProperty::validate() {
        if(displayName.empty()) {
            displayName = fieldName;
        }

        if(serializationName.empty()) {
            serializationName = fieldName;
        }
    }
}