//
// Created by jglrxavpok on 16/09/2026.
//

#include "StencilSettings.h"

#include <robin_hood.h>
#include <core/Macros.h>
#include <core/data/Hashes.h>
#include <core/utils/Lookup.hpp>

namespace Carrot::Render {
    static Lookup StencilOperationsTable = std::array {
        Carrot::LookupEntry(StencilOperation::AlwaysPass, "always_pass"),
        Carrot::LookupEntry(StencilOperation::Equal, "equal"),
    };

    const char* toString(const StencilOperation& op) {
        return StencilOperationsTable[op];
    }

    StencilOperation fromString(std::string_view str) {
        return StencilOperationsTable[str];
    }

    void StencilSettings::deserialise(const Carrot::DocumentElement& doc) {
        stencilEnabled = true;
        stencilValue = doc["reference"].getAsInt64();
        stencilOperation = fromString(doc["operation"].getAsString());
        stencilCompare = doc["compare"].getAsBool();
        stencilWrite = doc["write"].getAsBool();
    }

    Carrot::DocumentElement StencilSettings::serialise() const {
        Carrot::DocumentElement stencilInfo;
        stencilInfo["reference"] = stencilValue;
        stencilInfo["operation"] = toString(stencilOperation);
        stencilInfo["compare"] = stencilCompare;
        stencilInfo["write"] = stencilWrite;
        return stencilInfo;
    }
}

std::size_t std::hash<Carrot::Render::StencilSettings>::operator()(const Carrot::Render::StencilSettings& settings) const noexcept {
    std::size_t h = 0;
    Carrot::hash_combine(h, robin_hood::hash_int(settings.stencilCompare));
    Carrot::hash_combine(h, robin_hood::hash_int(settings.stencilEnabled));
    Carrot::hash_combine(h, robin_hood::hash_int(settings.stencilValue));
    Carrot::hash_combine(h, robin_hood::hash_int(settings.stencilWrite));
    Carrot::hash_combine(h, robin_hood::hash_int(static_cast<uint64_t>(settings.stencilOperation)));
    return h;
}
