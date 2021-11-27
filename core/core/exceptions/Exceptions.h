//
// Created by jglrxavpok on 17/09/2021.
//

#pragma once

#include <stdexcept>

namespace Carrot::Exceptions {
    class IrrecoverableError: public std::exception {
    public:
        explicit IrrecoverableError(const std::string& message): message("Irrecoverable error: " + message) {};

        const char* what() const override {
            return message.c_str();
        }

    private:
        std::string message;
    };
}