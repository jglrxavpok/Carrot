//
// Created by jglrxavpok on 12/03/2023.
//

#pragma once

#include <core/scripting/csharp/forward.h>
#include <mono/utils/mono-forward.h>
#include <mono/metadata/object-forward.h>
#include "mono/metadata/class.h"

namespace Carrot::Scripting {

    class CSField {
    public:
        explicit CSField(CSClass& parent, MonoDomain* appDomain, MonoClassField* field);

        CSObject get(const CSObject& instance) const;
        void set(const CSObject& instance, const CSObject& value);

    private:
        CSClass& parent;
        MonoDomain* appDomain = nullptr;
        MonoClassField* field = nullptr;
    };

} // Carrot::Scripting
