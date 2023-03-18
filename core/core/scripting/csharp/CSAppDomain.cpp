//
// Created by jglrxavpok on 15/03/2023.
//

#include "CSAppDomain.h"
#include <core/Macros.h>
#include <mono/metadata/appdomain.h>

namespace Carrot::Scripting {
    CSAppDomain::CSAppDomain(MonoDomain* domain): domain(domain) {
        mono_domain_set(domain, false);
    }

    CSAppDomain::~CSAppDomain() {
        mono_domain_set(mono_get_root_domain(), false);
        mono_domain_unload(domain);
    }

    MonoDomain* CSAppDomain::toMono() const {
        return domain;
    }
} // Carrot::Scripting