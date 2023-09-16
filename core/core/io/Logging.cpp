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

void Carrot::Log::log(Severity severity, const Category& category, const std::string& message, const std::source_location& src, std::ostream& out) {
/*#ifndef IS_DEBUG_BUILD
        if(severity == Severity::Debug)
            return;
#endif*/
    // TODO: write to file?

    const auto timestamp = std::chrono::system_clock::now() - getStartTime();

    out << Carrot::sprintf("[%s] [%s] (T %llu) %s [%s:%llu]\n", getSeverityString(severity), category.name.c_str(), timestamp.count(), message.c_str(), src.file_name(), (std::uint64_t)src.line());
//        out << "[" << getSeverityString(severity) << "] (T " << timestamp << ") " << message << '\n';

    getMessages().emplace_back(Message {
            .severity = severity,
            .timestamp = static_cast<std::uint64_t>(timestamp.count()),
            .message = message,

            .category = category,
            .sourceLoc = src,
    });
}

void Carrot::Assertions::printVerify(const std::string& condition, const std::string& message) {
    Carrot::Log::error(Carrot::sprintf("%s - %s", message.c_str(), condition.c_str()));
    Carrot::Log::flush();
}