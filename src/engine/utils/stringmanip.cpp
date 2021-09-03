//
// Created by jglrxavpok on 01/12/2020.
//

#include "stringmanip.h"
#include <cctype>
#include <codecvt>

vector<string> Carrot::splitString(const string& toSplit, const string& delimiter) {
    vector<string> parts{};

    string remaining = toSplit;
    while(!remaining.empty()) {
        auto nextDelimiter = remaining.find(delimiter);
        parts.push_back(remaining.substr(0, nextDelimiter));
        if(nextDelimiter == string::npos) {
            remaining = "";
        } else {
            remaining = remaining.substr(nextDelimiter+1);
        }
    }
    return parts;
}

std::string Carrot::toLowerCase(const string& str) {
    std::string result = str;
    for(auto& c : result) {
        c = std::tolower(c);
    }
    return result;
}

std::string Carrot::toUpperCase(const string& str) {
    std::string result = str;
    for(auto& c : result) {
        c = std::toupper(c);
    }
    return result;
}

std::string Carrot::toString(std::u32string_view u32str) {
    static std::wstring_convert<std::codecvt<char32_t,char,std::mbstate_t>,char32_t> converter;
    return converter.to_bytes(&u32str[0], &u32str[u32str.size()-1]);
}