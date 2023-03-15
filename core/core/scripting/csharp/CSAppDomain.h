//
// Created by jglrxavpok on 15/03/2023.
//

#pragma once

#include <mono/utils/mono-forward.h>

namespace Carrot::Scripting {

    class CSAppDomain {
    public:
        explicit CSAppDomain(MonoDomain* domain);
        ~CSAppDomain();

        MonoDomain* toMono() const;

    private:
        MonoDomain* domain = nullptr;
    };

} // Carrot::Scripting
