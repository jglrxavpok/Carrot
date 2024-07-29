//
// Created by jglrxavpok on 27/12/2021.
//

#pragma once

#include <cstdint>
#include <robin_hood.h>
#include <core/utils/CRC64.hpp>

namespace Carrot::Render {
    /**
     * \brief Key that is used to determine where a render packet should be consumed
     */
    struct PassName {
        std::uint64_t key;

        constexpr PassName(std::uint64_t value): key(value) {}

        constexpr bool operator==(const PassName& o) const {
            return key == o.key;
        }

        std::size_t hash() const {
            return robin_hood::hash_int(key);
        }
    };

    constexpr Carrot::Render::PassName makePassNameHash(std::string name) {
        return Carrot::CRC64(name.data(), name.size());
    }

#define MAKE_PASS_NAME(x) constexpr Carrot::Render::PassName x = makePassNameHash(#x);

    namespace PassEnum {
        MAKE_PASS_NAME(Undefined)
        MAKE_PASS_NAME(PrePassVisibilityBuffer)
        MAKE_PASS_NAME(VisibilityBuffer)
        MAKE_PASS_NAME(OpaqueGBuffer)
        MAKE_PASS_NAME(TransparentGBuffer)
        MAKE_PASS_NAME(Lighting)
        MAKE_PASS_NAME(Unlit) // rendered after lighting but before tonemapping
        MAKE_PASS_NAME(UI) // TODO: not implemented yet,
        MAKE_PASS_NAME(ImGui)
    }
}
