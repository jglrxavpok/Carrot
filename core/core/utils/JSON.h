//
// Created by jglrxavpok on 07/05/2021.
//

#pragma once

#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>
#include <string>
#include <vector>
#include <list>
#include <glm/glm.hpp>

namespace Carrot {
    /// Utility methods for JSON handling
    namespace JSON {
        // linked list to avoid moving elements in memory
        inline static std::list<std::string> saved{};

        /// Clear all string references saved
        inline static void clearReferences() {
            saved.clear();
        }

        /// Copy temporary string to a list and return a rapidjson-compatible StringRef
        ///  Extends the lifetime of a given string
        inline static rapidjson::GenericStringRef<char> makeRef(const std::string& toCopy) {
            saved.push_back(toCopy);
            return rapidjson::StringRef(saved.back().c_str());
        }

        template<glm::length_t len, typename Elem, glm::qualifier qualifier = glm::qualifier::defaultp>
        inline glm::vec<len, Elem, qualifier> read(const rapidjson::Value& source) {
            using vec = glm::vec<len, Elem, qualifier>;
            vec v;
            auto array = source.GetArray();
            for (glm::length_t i = 0; i < len; ++i) {
                if constexpr(std::is_same_v<Elem, float>) {
                    v[i] = array[i].GetFloat();
                } else if constexpr(std::is_same_v<Elem, double>){
                    v[i] = array[i].GetDouble();
                } else {
                    static_assert(!std::is_same_v<Elem, double> && !std::is_same_v<Elem, float>, "Unknown type");
                }
            }
            return v;
        }

        template<glm::length_t len, typename Elem, glm::qualifier qualifier = glm::qualifier::defaultp>
        inline rapidjson::Value write(const glm::vec<len, Elem, qualifier>& vec, rapidjson::Document::AllocatorType& allocator) {
            rapidjson::Value arr(rapidjson::kArrayType);
            for (glm::length_t i = 0; i < len; ++i) {
                arr.PushBack(vec[i], allocator);
            }
            return arr;
        }

        template<glm::length_t len, typename Elem, glm::qualifier qualifier = glm::qualifier::defaultp>
        inline rapidjson::Value write(const glm::vec<len, Elem, qualifier>& vec, rapidjson::Document& allocator) {
            return write<len, Elem, qualifier>(vec, allocator.GetAllocator());
        }
    }
}