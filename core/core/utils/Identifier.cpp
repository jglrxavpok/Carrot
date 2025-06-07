//
// Created by jglrxavpok on 07/06/25.
//

#include "Identifier.h"

#include <utility>

#include <core/utils/CRC64.hpp>

namespace Carrot {

    Identifier::Identifier() {
        update("");
    }

    Identifier::Identifier(std::string_view contents) {
        update(contents);
    }

    Identifier::Identifier(const Identifier& o) {
        *this = o;
    }

    Identifier::Identifier(Identifier&& o) noexcept {
        *this = std::move(o);
    }

    std::size_t Identifier::getHash() const {
        return crc;
    }

    Identifier::operator std::string_view() const {
        return storage;
    }

    bool Identifier::operator==(const Identifier& o) const {
        return crc == o.crc && storage == o.storage;
    }

    Identifier& Identifier::operator=(const Identifier& o) = default;

    Identifier& Identifier::operator=(Identifier&& o) noexcept {
        storage = std::move(o.storage);
        crc = std::exchange(o.crc, 0);
        return *this;
    }

    void Identifier::update(std::string_view text) {
        storage = text;
        crc = CRC64(storage.data(), storage.size());
    }

} // Carrot

std::size_t std::hash<Carrot::Identifier>::operator()(const Carrot::Identifier& identifier) const noexcept {
    return identifier.getHash();
}