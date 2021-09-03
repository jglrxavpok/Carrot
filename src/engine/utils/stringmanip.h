//
// Created by jglrxavpok on 01/12/2020.
//

#pragma once
#include <vector>
#include <string>
#include <memory>
#include <utility>

using namespace std;

namespace Carrot {
    vector<string> splitString(const string& toSplit, const string& delimiter);

    std::string toLowerCase(const string& str);
    std::string toUpperCase(const string& str);

    std::string toString(std::u32string_view u32str);

    template<typename... Args>
    inline std::string sprintf(std::string_view format, Args... args) {
        int size = std::snprintf(nullptr, 0, format.data(), args...) +1;
        std::unique_ptr<char[]> buffer = std::make_unique<char[]>(size);
        std::snprintf(buffer.get(), size, format.data(), args...);
        return std::string(buffer.get());
    }
}