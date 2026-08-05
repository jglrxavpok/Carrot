//
// Created by jglrxavpok on 06/10/2021.
//

#include "Identifiable.h"

std::atomic<Carrot::ComponentID> LastComponentID{0};
Carrot::Async::ParallelMap<Carrot::ComponentID, std::string>& Carrot::GetIdentifiableNames() {
    static Carrot::Async::ParallelMap<Carrot::ComponentID, std::string> r;
    return r;
}
Carrot::Async::ParallelMap<std::string, Carrot::ComponentID>& Carrot::GetIdentifiableIDs() {
    static Carrot::Async::ParallelMap<std::string, Carrot::ComponentID> r;
    return r;
}

std::optional<Carrot::ComponentID> Carrot::getIDFromName(std::string_view name) {
    std::string key = std::string(name);
    auto* it = GetIdentifiableIDs().find(key);
    if(!it) {
        return std::optional<Carrot::ComponentID>();
    }

    return *it;
}

Carrot::ComponentID Carrot::requestComponentID() {
    return LastComponentID++;
}