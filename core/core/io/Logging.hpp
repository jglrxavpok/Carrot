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
#include <cassert>
#include <strstream>
#include "core/utils/stringmanip.h"

namespace Carrot::Log {
    class LogError: public std::exception {
    public:
        explicit LogError(std::string message): message(std::move(message)) {}

        const char *what() const noexcept override {
            return message.c_str();
        }

    private:
        std::string message;
    };

    enum class Severity {
        Debug,
        Info,
        Warning,
        Error,
    };

    struct Message {
        Severity severity;
        std::uint64_t timestamp;
        std::string message;
    };

    std::list<Message>& getMessages();
    const std::chrono::system_clock::time_point& getStartTime();

    inline const char* getSeverityString(Severity severity) {
        switch (severity) {
            case Severity::Debug:
                return "Debug";
            case Severity::Info:
                return "Info";
            case Severity::Warning:
                return "Warning";
            case Severity::Error:
                return "Error";
        }
        assert(false);
        throw std::runtime_error("Unknown severity. Have you tested your code in debug?");
    }

    inline void log(Severity severity, const std::string& message, std::ostream& out = std::cout) {
/*#ifndef IS_DEBUG_BUILD
        if(severity == Severity::Debug)
            return;
#endif*/
        // TODO: write to file?

        const auto timestamp = std::chrono::system_clock::now() - getStartTime();

        out << Carrot::sprintf("[%s] (T %llu) %s\n", getSeverityString(severity), timestamp.count(), message.c_str());
//        out << "[" << getSeverityString(severity) << "] (T " << timestamp << ") " << message << '\n';

        getMessages().emplace_back(Message {
            .severity = severity,
            .timestamp = static_cast<std::uint64_t>(timestamp.count()),
            .message = message,
        });
    }

    inline void flush() {
        std::cout.flush();
        std::cerr.flush();
    }

    inline void debug(const std::string& message) {
        log(Severity::Debug, message);
    }

    inline void info(const std::string& message) {
        log(Severity::Info, message);
    }

    inline void warn(const std::string& message) {
        log(Severity::Warning, message);
    }

    inline void error(const std::string& message) {
        log(Severity::Error, message, std::cerr);
    }

    using LogFunctionType = void(const std::string& message);

    template<LogFunctionType logFunction, typename Arg0, typename... Args>
    void formattedLog(const std::string& format, Arg0 arg0, Args... args) {
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
    void debug(const std::string& format, Arg0 arg0, Args... args) { formattedLog<debug>(format, std::forward<Arg0>(arg0), std::forward<Args>(args)...); };

    template<typename Arg0, typename... Args>
    void info(const std::string& format, Arg0 arg0, Args... args) { formattedLog<info>(format, std::forward<Arg0>(arg0), std::forward<Args>(args)...); };

    template<typename Arg0, typename... Args>
    void warn(const std::string& format, Arg0 arg0, Args... args) { formattedLog<warn>(format, std::forward<Arg0>(arg0), std::forward<Args>(args)...); };

    template<typename Arg0, typename... Args>
    void error(const std::string& format, Arg0 arg0, Args... args) { formattedLog<error>(format, std::forward<Arg0>(arg0), std::forward<Args>(args)...); };
}