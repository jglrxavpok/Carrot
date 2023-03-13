//
// Created by jglrxavpok on 11/03/2023.
//

#pragma once

#include <core/scripting/csharp/forward.h>
#include <mono/utils/mono-forward.h>
#include <mono/metadata/object-forward.h>
#include "mono/metadata/class.h"

namespace Carrot::Scripting {

    class CSProperty {
    public:
        explicit CSProperty(CSClass& parent, MonoProperty* property);

        CSObject get(const CSObject& instance) const;
        void set(const CSObject& instance, const CSObject& value);

    private:
        CSClass& parent;
        MonoProperty* property = nullptr;
        MonoMethod* getter = nullptr;
        MonoMethod* setter = nullptr;
    };

} // Carrot::Scripting
