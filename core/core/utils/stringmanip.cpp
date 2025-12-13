//
// Created by jglrxavpok on 01/12/2020.
//

#include "stringmanip.h"
#include <cctype>
#include <codecvt>
#include <cstring>
#include <core/Macros.h>

int Carrot::icompareAscii(const std::string_view& a, const std::string_view& b) {
    i64 minSize = std::min(a.size(), b.size());
    for (i64 index = 0; index < minSize; index++) {
        const char toUppercaseAsciiA = std::toupper(a[index]);
        const char toUppercaseAsciiB = std::toupper(b[index]);
        if (toUppercaseAsciiA < toUppercaseAsciiB) {
            return -1;
        } else if (toUppercaseAsciiA > toUppercaseAsciiB) {
            return 1;
        }
    }
    if (a.size() == b.size()) {
        return 0;
    }
    return a.size() < b.size() ? -1 : 1;
}

std::vector<std::string> Carrot::splitString(const std::string& toSplit, const std::string& delimiter) {
    std::vector<std::string> parts{};

    std::string remaining = toSplit;
    while(!remaining.empty()) {
        auto nextDelimiter = remaining.find(delimiter);
        parts.push_back(remaining.substr(0, nextDelimiter));
        if(nextDelimiter == std::string::npos) {
            remaining = "";
        } else {
            remaining = remaining.substr(nextDelimiter+1);
        }
    }
    return parts;
}

std::string Carrot::joinStrings(const std::span<std::string_view>& toJoin, std::string_view joiner) {
    if(toJoin.empty()) {
        return "";
    }
    std::size_t totalSize = 0;
    for(const auto& part : toJoin) {
        totalSize += part.size();
    }

    std::string result;
    result.reserve(totalSize + (toJoin.size() - 1) * joiner.size());
    bool first = true;
    for(const auto& part : toJoin) {
        if(!first) {
            result += joiner;
        }
        result += part;

        first = false;
    }
    return result;
}

std::string Carrot::toLowerCase(const std::string& str) {
    std::string result = str;
    for(auto& c : result) {
        c = std::tolower(c);
    }
    return result;
}

std::string Carrot::toUpperCase(const std::string& str) {
    std::string result = str;
    for(auto& c : result) {
        c = std::toupper(c);
    }
    return result;
}

struct WStringConverter32: std::codecvt_utf8<char32_t> {
    ~WStringConverter32() = default;
};

std::string Carrot::toString(std::u8string_view wstr) {
    if(wstr.empty())
        return "";

    // assume char8_t = char
    std::string str;
    str.resize(wstr.size());
    memcpy(str.data(), wstr.data(), str.size());
    return str;
}

std::string Carrot::toString(std::u32string_view u32str) {
    if(u32str.empty())
        return "";
    static std::wstring_convert<WStringConverter32, char32_t> converter;
    return converter.to_bytes(&u32str[0], &u32str[u32str.size()-1] + sizeof(char32_t));
}

std::u32string Carrot::toU32String(std::string_view str) {
    if(str.empty())
        return U"";
    static std::wstring_convert<WStringConverter32, char32_t> converter;
    return converter.from_bytes(str.data());
}

std::string Carrot::replace(const std::string& str, const std::string& toReplace, const std::string_view& toReplaceWith) {
    if(str.empty())
        return str;
    if(toReplace.empty())
        return str;

    const std::size_t tokenSize = toReplace.size();
    std::size_t cursor = 0;

    std::string result = "";
    result.reserve(str.size());
    while(cursor < str.size()-tokenSize) {
        if(strncmp(&str[cursor], toReplace.c_str(), tokenSize) == 0) { // found a match
            result += toReplaceWith;
            cursor += tokenSize;
        } else {
            result += str[cursor];
            cursor++;
        }
    }

    result += str.substr(cursor); // remaining of string

    return result;
}