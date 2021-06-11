//
// Created by jglrxavpok on 11/06/2021.
//

#pragma once

#include <stdexcept>

namespace Carrot::Assertions {
    class Error: public std::exception {
    public:
        explicit Error(std::string condition, std::string message, std::string file, std::string function, int line) {
            whatMessage = condition + ", " + message + " in " + file + " " + function + " L" + std::to_string(line);
        }

        const char *what() const override {
            return whatMessage.c_str();
        }

    private:
        std::string whatMessage;
    };
}

#define runtimeAssert(condition, message) \
    if(!(condition)) throw Carrot::Assertions::Error(#condition, message, __FILE__, __FUNCTION__, __LINE__);