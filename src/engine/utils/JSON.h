//
// Created by jglrxavpok on 07/05/2021.
//

#pragma once

#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>
#include <string>
#include <vector>

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
    }
}