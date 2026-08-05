//
// Created by jglrxavpok on 20/02/2021.
//

#pragma once

#include <cstddef>
#include <atomic>
#include <utility>
#include <map>
#include <core/async/ParallelMap.hpp>

namespace Carrot {

    using ComponentID = std::size_t;

    template<typename Type>
    struct Identifiable {
        static const ComponentID getID();

        static const char* getStringRepresentation();
    };

    template<typename Type>
    concept IsIdentifiable = requires()
    {
        { Type::getID() } -> std::convertible_to<ComponentID>;
        { Type::getStringRepresentation() } -> std::convertible_to<const char*>;
    };

    std::optional<Carrot::ComponentID> getIDFromName(std::string_view name);

    Carrot::Async::ParallelMap<Carrot::ComponentID, std::string>& GetIdentifiableNames();
    Carrot::Async::ParallelMap<std::string, Carrot::ComponentID>& GetIdentifiableIDs();

    Carrot::ComponentID requestComponentID();
}
extern std::atomic<Carrot::ComponentID> LastComponentID;

#include "Identifiable.ipp"