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
            } else if constexpr(std::is_same_v<Elem, double>){
                v[i] = array[i].getAsDouble();
            } else if constexpr(std::is_same_v<Elem, bool>){
                v[i] = array[i].getAsBool();
            } else {
                static_assert(!std::is_same_v<Elem, double> && !std::is_same_v<Elem, float>, "Unknown type");
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