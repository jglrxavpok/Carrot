//
// Created by jglrxavpok on 07/06/2021.
//
#include "Logging.hpp"

std::list<Carrot::Log::Message>& Carrot::Log::getMessages() {
    static std::list<Carrot::Log::Message> messages;

    return messages;
}

const std::chrono::system_clock::time_point& Carrot::Log::getStartTime() {
    static auto start = std::chrono::system_clock::now();
    return start;
}