//
// Created by jglrxavpok on 15/03/2023.
//

#pragma once

#include <mono/utils/mono-forward.h>

#include "core/memory/ThreadLocal.hpp"
#include "mono/metadata/object.h"

namespace Carrot::Scripting {

    class CSAppDomain {
    public:
        explicit CSAppDomain(MonoDomain* domain);
        ~CSAppDomain();

        void setCurrent();
        void registerCurrentThread();
        void unregisterCurrentThread();
        void prepareForUnload();
        MonoThread* getCurrentThread();

        MonoDomain* toMono() const;

    private:
        ThreadLocal<MonoThread*> pMonoThread;
        bool preparedForUnload = false;
        MonoDomain* domain = nullptr;
    };

} // Carrot::Scripting
