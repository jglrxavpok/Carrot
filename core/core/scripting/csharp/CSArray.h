//
// Created by jglrxavpok on 12/03/2023.
//

#pragma once

#include <cstddef>
#include <cstdint>
#include <core/scripting/csharp/forward.h>
#include <mono/utils/mono-forward.h>
#include <mono/metadata/object-forward.h>
#include "mono/metadata/class.h"
#include "mono/metadata/object.h"

namespace Carrot::Scripting {

    class CSArray {
    public:
        explicit CSArray(CSClass& parent, MonoDomain* appDomain, MonoArray* array);
        ~CSArray();

        void set(std::size_t index, const CSObject& value);
        CSObject get(std::size_t index);

        MonoArray* toMono();

    private:
        CSClass& parent;
        MonoDomain* appDomain = nullptr;
        MonoArray* array = nullptr;
        std::uint32_t gcHandle = 0;
    };

} // Carrot::Scripting
