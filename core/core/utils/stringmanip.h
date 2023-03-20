//
// Created by jglrxavpok on 01/12/2020.
//

#pragma once
#include <vector>
#include <string>
#include <memory>
#include <utility>
#include <span>

namespace Carrot {
    std::vector<std::string> splitString(const std::string& toSplit, const std::string& delimiter);
    std::string joinStrings(const std::span<std::string_view>& toJoin, std::string_view joiner);

    std::string toLowerCase(const std::string& str);
    std::string toUpperCase(const std::string& str);

    std::string toString(std::u32string_view u32str);
    std::string toString(std::u8string_view wstr);
    std::u32string toU32String(std::string_view str);

    template<typename... Args>
    inline std::string sprintf(std::string_view format, Args... args) {
        int size = std::snprintf(nullptr, 0, format.data(), args...) +1;
        std::unique_ptr<char[]> buffer = std::make_unique<char[]>(size);
        std::snprintf(buffer.get(), size, format.data(), args...);
        return std::string(buffer.get());
    }

    /**
     * Replace occurrences of 'toReplace' inside 'str' with 'toReplaceWith'
     */
    std::string replace(const std::string& str, const std::string& toReplace, const std::string_view& toReplaceWith);
}