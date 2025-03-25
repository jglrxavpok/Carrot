//
// Created by jglrxavpok on 15/03/2023.
//

#include "CSAppDomain.h"
#include <core/Macros.h>
#include <mono/metadata/appdomain.h>

#include "mono/metadata/threads.h"

namespace Carrot::Scripting {
    CSAppDomain::CSAppDomain(MonoDomain* domain): domain(domain) {

    }

    void CSAppDomain::setCurrent() {
        mono_domain_set(domain, false);
    }

    MonoThread* CSAppDomain::getCurrentThread() {
        return pMonoThread.get();
    }

    void CSAppDomain::registerCurrentThread() {
        pMonoThread.get() = mono_thread_attach(domain);
    }

    void CSAppDomain::unregisterCurrentThread() {
        mono_thread_exit();
//        mono_thread_detach(pMonoThread.get());
    }

    void CSAppDomain::prepareForUnload() {
        preparedForUnload = true;
        mono_domain_set(mono_get_root_domain(), false);
    }

    CSAppDomain::~CSAppDomain() {
        verify(preparedForUnload, "Missing call to 'prepareForUnload'");
        mono_domain_unload(domain);
    }

    MonoDomain* CSAppDomain::toMono() const {
        return domain;
    }
} // Carrot::Scripting