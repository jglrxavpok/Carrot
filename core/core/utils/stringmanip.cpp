//
// Created by jglrxavpok on 01/12/2020.
//

#include "stringmanip.h"
#include <cctype>
#include <codecvt>
#include <core/Macros.h>

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

std::string Carrot::toString(std::u8string_view wstr) {
    static std::wstring_convert<std::codecvt<char8_t,char,std::mbstate_t>,char8_t> converter;
    return converter.to_bytes(&wstr[0], &wstr[wstr.size()-1] + sizeof(char8_t));
}

std::string Carrot::toString(std::u32string_view u32str) {
    static std::wstring_convert<std::codecvt<char32_t,char,std::mbstate_t>,char32_t> converter;
    return converter.to_bytes(&u32str[0], &u32str[u32str.size()-1] + sizeof(char32_t));
}

std::u32string Carrot::toU32String(std::string_view str) {
    static std::wstring_convert<std::codecvt<char32_t,char,std::mbstate_t>,char32_t> converter;
    return converter.from_bytes(str.data());
}