//
// Created by jglrxavpok on 02/06/2021.
//

#pragma once

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <list>

namespace Carrot::Log {
    class LogError: public std::exception {
    public:
        explicit LogError(std::string message): message(std::move(message)) {}

        const char *what() const override {
            return message.c_str();
        }

    private:
        std::string message;
    };

    struct LogMessage {
        std::string severity;
        std::uint64_t timestamp;
        std::string message;
    };

    std::list<LogMessage>& getMessages();

    inline void log(const std::string& severity, const std::string& message, std::ostream& out = std::cout) {
        // TODO: write to file?
        const auto currentTime = std::chrono::system_clock::now();

        out << "[" << severity << "] (T " << currentTime.time_since_epoch().count() << ") " << message << '\n';

        getMessages().emplace_back(LogMessage {
            .severity = severity,
            .timestamp = static_cast<uint64_t>(currentTime.time_since_epoch().count()),
            .message = message,
        });
    }

    inline void info(const std::string& message) {
        log("INFO", message);
    }

    inline void warn(const std::string& message) {
        log("WARN", message);
    }

    inline void error(const std::string& message) {
        log("ERROR", message, std::cerr);
    }

    using LogFunctionType = void(const std::string& message);

    template<LogFunctionType logFunction, typename Arg0, typename... Args>
    void log(const std::string& format, Arg0 arg0, Args... args) {
        auto size = std::snprintf(nullptr, 0, format.c_str(), arg0, args...) + 1;
        if(size <= 0) {
            throw LogError("Failed to format message " + format);
        }

        auto buffer = std::make_unique<char[]>(size);
        std::snprintf(buffer.get(), size, format.c_str(), arg0, args...);

        std::string formatResult{buffer.get(), buffer.get() + size - 1};

        logFunction(formatResult);
    }

    template<typename Arg0, typename... Args>
    void info(const std::string& format, Arg0 arg0, Args... args) { log<info>(format, std::forward<Arg0>(arg0), std::forward<Args>(args)...); };

    template<typename Arg0, typename... Args>
    void warn(const std::string& format, Arg0 arg0, Args... args) { log<warn>(format, std::forward<Arg0>(arg0), std::forward<Args>(args)...); };

    template<typename Arg0, typename... Args>
    void error(const std::string& format, Arg0 arg0, Args... args) { log<error>(format, std::forward<Arg0>(arg0), std::forward<Args>(args)...); };
}