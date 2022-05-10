//
// Created by jglrxavpok on 11/06/2021.
//

#pragma once

#include <stdexcept>
#include <string>

namespace Carrot::Assertions {
    class Error: public std::exception {
    public:
        explicit Error(std::string condition, std::string message, std::string file, std::string function, int line) {
            whatMessage = condition + ", " + message + " in " + file + " " + function + " L" + std::to_string(line);
        }

        const char *what() const noexcept override {
            return whatMessage.c_str();
        }

    private:
        std::string whatMessage;
    };

    void printVerify(const std::string& condition, const std::string& message);
}

#define verify(condition, message) \
    if(!(condition)) {             \
        Carrot::Assertions::printVerify(#condition, message);       \
        throw Carrot::Assertions::Error(#condition, message, __FILE__, __FUNCTION__, __LINE__); \
    } else {}

#define verifyTerminate(condition, message) \
    if(!(condition)) {             \
        Carrot::Assertions::printVerify(#condition, message);       \
        std::terminate(); \
    } else {}
