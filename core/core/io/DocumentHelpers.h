//
// Created by jglrxavpok on 14/02/2025.
//

#pragma once
#include <core/io/Document.h>

namespace Carrot::DocumentHelpers {
    template<glm::length_t len, typename Elem, glm::qualifier qualifier = glm::qualifier::defaultp>
            inline glm::vec<len, Elem, qualifier> read(const Carrot::DocumentElement& source) {
        using vec = glm::vec<len, Elem, qualifier>;
        vec v;
        auto array = source.getAsArray();
        for (glm::length_t i = 0; i < len; ++i) {
            if constexpr(std::is_same_v<Elem, float>) {
                v[i] = array[i].getAsDouble();
            } else if constexpr(std::is_same_v<Elem, double>) {
                v[i] = array[i].getAsDouble();
            } else if constexpr(std::is_same_v<Elem, bool>) {
                v[i] = array[i].getAsBool();
            } else if constexpr(std::is_same_v<Elem, i64>) {
                v[i] = array[i].getAsInt64();
            } else if constexpr(std::is_same_v<Elem, u32>) {
                v[i] = static_cast<u32>(array[i].getAsInt64());
            } else if constexpr(std::is_same_v<Elem, i32>) {
                v[i] = static_cast<i32>(array[i].getAsInt64());
            } else {
                TODO;
            }
        }
        return v;
    }

    template<glm::length_t len, typename Elem, glm::qualifier qualifier = glm::qualifier::defaultp>
    inline Carrot::DocumentElement write(const glm::vec<len, Elem, qualifier>& vec) {
        Carrot::DocumentElement arr{Carrot::DocumentType::Array};
        for (glm::length_t i = 0; i < len; ++i) {
            arr.pushBack(vec[i]);
        }
        return arr;
    }
}