//
// Created by jglrxavpok on 16/09/2026.
//

#pragma once
#include <core/io/Document.h>
#include <core/utils/Identifier.h>

namespace Carrot::Render {

    enum class StencilOperation {
        Equal,
        AlwaysPass,
    };

    struct StencilSettings {
        StencilOperation op = StencilOperation::AlwaysPass;
        bool stencilEnabled = false; // used to apply stencil operations to the entire viewport
        bool stencilCompare = false; // unused if stencilEnabled == false
        bool stencilWrite = false; // unused if stencilEnabled == false
        std::optional<std::unordered_map<Carrot::Identifier, bool>> incrementPerViewport; // unused if stencilEnabled == false. Does nothing if stencilWrite == false. If false, writing to the stencil will use stencilValue to write. If true, writing to the stencil will take the current value and increment it
        std::optional<std::unordered_map<Carrot::Identifier, bool>> stencilComparePerViewport; // unused if stencilEnabled == false. Does nothing if stencilWrite == false. If false, writing to the stencil will use stencilValue to write. If true, writing to the stencil will take the current value and increment it
        StencilOperation stencilOperation = StencilOperation::AlwaysPass; // unused if stencilEnabled == false
        u8 stencilValue = 0; // unused if stencilEnabled == false. Value to compare or write depending on stencilOperation
        std::optional<std::unordered_map<Carrot::Identifier, u8>> stencilValuePerViewport; // unused if stencilEnabled == false. Value to compare or write depending on stencilOperation
        std::optional<std::unordered_map<Carrot::Identifier, StencilOperation>> stencilOperationPerViewport; // unused if stencilEnabled == false. Value to compare or write depending on stencilOperation

        bool operator==(const StencilSettings&) const = default;
        bool operator!=(const StencilSettings&) const = default;

        void deserialise(const Carrot::DocumentElement& doc);
        Carrot::DocumentElement serialise() const;
    };

    const char* toString(const StencilOperation& op);
    StencilOperation fromString(std::string_view str);
}

namespace std {
    template<>
    struct hash<Carrot::Render::StencilSettings> {
        std::size_t operator()(const Carrot::Render::StencilSettings&) const noexcept;
    };
}