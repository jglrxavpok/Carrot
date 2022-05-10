//
// Created by jglrxavpok on 07/06/2021.
//
#include "Logging.hpp"
#include "core/utils/Assert.h"

std::list<Carrot::Log::Message>& Carrot::Log::getMessages() {
    static std::list<Carrot::Log::Message> messages;

    return messages;
}

const std::chrono::system_clock::time_point& Carrot::Log::getStartTime() {
    static auto start = std::chrono::system_clock::now();
    return start;
}

void Carrot::Assertions::printVerify(const std::string& condition, const std::string& message) {
    Carrot::Log::error(Carrot::sprintf("%s - %s", message.c_str(), condition.c_str()));
    Carrot::Log::flush();
}