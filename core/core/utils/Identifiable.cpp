//
// Created by jglrxavpok on 06/10/2021.
//

#include "Identifiable.h"

std::atomic<Carrot::ComponentID> LastComponentID{0};
Carrot::Async::ParallelMap<Carrot::ComponentID, std::string> Carrot::IdentifiableNames;
Carrot::Async::ParallelMap<std::string, Carrot::ComponentID> Carrot::IdentifiableIDs;

std::optional<Carrot::ComponentID> Carrot::getIDFromName(std::string_view name) {
    std::string key = std::string(name);
    auto* it = IdentifiableIDs.find(key);
    if(!it) {
        return std::optional<Carrot::ComponentID>();
    }

    return *it;
}