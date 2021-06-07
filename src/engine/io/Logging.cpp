//
// Created by jglrxavpok on 07/06/2021.
//
#include "Logging.hpp"

std::list<Carrot::Log::LogMessage>& Carrot::Log::getMessages() {
    static std::list<Carrot::Log::LogMessage> messages;

    return messages;
}