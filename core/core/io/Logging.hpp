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
#include <source_location>

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

    struct Category {
        std::string name;
    };

    struct Message {
        Severity severity;
        std::uint64_t timestamp;
        std::string message;

        Category category;
        std::source_location sourceLoc;
    };

    static const Category defaultCategory { "Default" };

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

    inline void log(Severity severity, const Category& category, const std::string& message, const std::source_location& src, std::ostream& out = std::cout) {
/*#ifndef IS_DEBUG_BUILD
        if(severity == Severity::Debug)
            return;
#endif*/
        // TODO: write to file?

        const auto timestamp = std::chrono::system_clock::now() - getStartTime();

        out << Carrot::sprintf("[%s] [%s] (T %llu) %s [%s : %llu]\n", getSeverityString(severity), category.name.c_str(), timestamp.count(), message.c_str(), src.file_name(), (std::uint64_t)src.line());
//        out << "[" << getSeverityString(severity) << "] (T " << timestamp << ") " << message << '\n';

        getMessages().emplace_back(Message {
            .severity = severity,
            .timestamp = static_cast<std::uint64_t>(timestamp.count()),
            .message = message,

            .category = category,
            .sourceLoc = src,
        });
    }

    inline void flush() {
        std::cout.flush();
        std::cerr.flush();
    }

    inline void debug_log(const std::string& message, const Category& category, const std::source_location& src) {
        log(Severity::Debug, category, message, src);
    }

    inline void info_log(const std::string& message, const Category& category, const std::source_location& src) {
        log(Severity::Info, category, message, src);
    }

    inline void warn_log(const std::string& message, const Category& category, const std::source_location& src) {
        log(Severity::Warning, category, message, src);
    }

    inline void error_log(const std::string& message, const Category& category, const std::source_location& src) {
        log(Severity::Error, category,message, src, std::cerr);
    }

    using LogFunctionType = void(const std::string& message, const Category& category, const std::source_location& src);

    template<LogFunctionType logFunction, typename Arg0, typename... Args>
    void formattedLog(const Category& category, const std::source_location& sourceLocation, const std::string& format, Arg0 arg0, Args... args) {
        auto size = std::snprintf(nullptr, 0, format.c_str(), arg0, args...) + 1;
        if(size <= 0) {
            throw LogError("Failed to format message " + format);
        }

        auto buffer = std::make_unique<char[]>(size);
        std::snprintf(buffer.get(), size, format.c_str(), arg0, args...);

        std::string formatResult{buffer.get(), buffer.get() + size - 1};

        logFunction(formatResult, category, sourceLocation);
    }

    template<LogFunctionType logFunction, typename Arg0, typename... Args>
    void formattedLog(const std::string& format, Arg0 arg0, Args... args) {
        formattedLog<logFunction>(defaultCategory, std::source_location::current(), format, std::forward<Arg0>(arg0), std::forward<Args>(args)...);
    }

    template<LogFunctionType logFunction, typename Arg0, typename... Args>
    void cformattedLog(const Category& category, const std::source_location& src, const std::string& format, Arg0 arg0, Args... args) {
        formattedLog<logFunction>(category, src, format, std::forward<Arg0>(arg0), std::forward<Args>(args)...);
    }

#define DEFINE_LOG_FUNCTION_SUBHELPER6(NAME) \
    template<typename Arg0_t, typename Arg1_t, typename Arg2_t, typename Arg3_t, typename Arg4_t, typename Arg5_t> \
    void NAME (const Category& category, const std::string& format, Arg0_t arg0, Arg1_t arg1, Arg2_t arg2, Arg3_t arg3, Arg4_t arg4, Arg5_t arg5, const std::source_location& sourceLoc = std::source_location::current()) { cformattedLog<NAME ## _log>(category, sourceLoc, format, std::forward<Arg0_t>(arg0), std::forward<Arg1_t>(arg1), std::forward<Arg2_t>(arg2), std::forward<Arg3_t>(arg3), std::forward<Arg4_t>(arg4), std::forward<Arg5_t>(arg5)); } \
    template<typename Arg0_t, typename Arg1_t, typename Arg2_t, typename Arg3_t, typename Arg4_t, typename Arg5_t> \
    void NAME (const std::string& format, Arg0_t arg0, Arg1_t arg1, Arg2_t arg2, Arg3_t arg3, Arg4_t arg4, Arg5_t arg5, const std::source_location& sourceLoc = std::source_location::current()) { NAME(defaultCategory, format, std::forward<Arg0_t>(arg0), std::forward<Arg1_t>(arg1), std::forward<Arg2_t>(arg2), std::forward<Arg3_t>(arg3), std::forward<Arg4_t>(arg4), std::forward<Arg5_t>(arg5), sourceLoc); } \

#define DEFINE_LOG_FUNCTION_SUBHELPER5(NAME) \
    template<typename Arg0_t, typename Arg1_t, typename Arg2_t, typename Arg3_t, typename Arg4_t> \
    void NAME (const Category& category, const std::string& format, Arg0_t arg0, Arg1_t arg1, Arg2_t arg2, Arg3_t arg3, Arg4_t arg4, const std::source_location& sourceLoc = std::source_location::current()) { cformattedLog<NAME ## _log>(category, sourceLoc, format, std::forward<Arg0_t>(arg0), std::forward<Arg1_t>(arg1), std::forward<Arg2_t>(arg2), std::forward<Arg3_t>(arg3), std::forward<Arg4_t>(arg4)); } \
    template<typename Arg0_t, typename Arg1_t, typename Arg2_t, typename Arg3_t, typename Arg4_t> \
    void NAME (const std::string& format, Arg0_t Arg0, Arg1_t Arg1, Arg2_t Arg2, Arg3_t Arg3, Arg4_t Arg4, const std::source_location& sourceLoc = std::source_location::current()) { NAME(defaultCategory, format, Arg0, Arg1, Arg2, Arg3, Arg4, sourceLoc); }

#define DEFINE_LOG_FUNCTION_SUBHELPER4(NAME) \
    template<typename Arg0_t, typename Arg1_t, typename Arg2_t, typename Arg3_t> \
    void NAME (const Category& category, const std::string& format, Arg0_t arg0, Arg1_t arg1, Arg2_t arg2, Arg3_t arg3, const std::source_location& sourceLoc = std::source_location::current()) { cformattedLog<NAME ## _log>(category, sourceLoc, format, std::forward<Arg0_t>(arg0), std::forward<Arg1_t>(arg1), std::forward<Arg2_t>(arg2), std::forward<Arg3_t>(arg3)); } \
    template<typename Arg0_t, typename Arg1_t, typename Arg2_t, typename Arg3_t> \
    void NAME (const std::string& format, Arg0_t Arg0, Arg1_t Arg1, Arg2_t Arg2, Arg3_t Arg3, const std::source_location& sourceLoc = std::source_location::current()) { NAME(defaultCategory, format, Arg0, Arg1, Arg2, Arg3, sourceLoc); }

#define DEFINE_LOG_FUNCTION_SUBHELPER3(NAME) \
    template<typename Arg0_t, typename Arg1_t, typename Arg2_t> \
    void NAME (const Category& category, const std::string& format, Arg0_t arg0, Arg1_t arg1, Arg2_t arg2, const std::source_location& sourceLoc = std::source_location::current()) { cformattedLog<NAME ## _log>(category, sourceLoc, format, std::forward<Arg0_t>(arg0), std::forward<Arg1_t>(arg1), std::forward<Arg2_t>(arg2)); } \
    template<typename Arg0_t, typename Arg1_t, typename Arg2_t> \
    void NAME (const std::string& format, Arg0_t Arg0, Arg1_t Arg1, Arg2_t Arg2, const std::source_location& sourceLoc = std::source_location::current()) { NAME(defaultCategory, format, Arg0, Arg1, Arg2, sourceLoc); }

#define DEFINE_LOG_FUNCTION_SUBHELPER2(NAME) \
    template<typename Arg0_t, typename Arg1_t> \
    void NAME (const Category& category, const std::string& format, Arg0_t arg0, Arg1_t arg1, const std::source_location& sourceLoc = std::source_location::current()) { cformattedLog<NAME ## _log>(category, sourceLoc, format, std::forward<Arg0_t>(arg0), std::forward<Arg1_t>(arg1)); } \
    template<typename Arg0_t, typename Arg1_t> \
    void NAME (const std::string& format, Arg0_t Arg0, Arg1_t Arg1, const std::source_location& sourceLoc = std::source_location::current()) { NAME(defaultCategory, format, Arg0, Arg1, sourceLoc); }

#define DEFINE_LOG_FUNCTION_SUBHELPER1(NAME) \
    template<typename Arg0_t> \
    void NAME (const Category& category, const std::string& format, Arg0_t arg0, const std::source_location& sourceLoc = std::source_location::current()) { cformattedLog<NAME ## _log>(category, sourceLoc, format, std::forward<Arg0_t>(arg0)); } \
    template<typename Arg0_t> \
    void NAME (const std::string& format, Arg0_t Arg0, const std::source_location& sourceLoc = std::source_location::current()) { NAME(defaultCategory, format, Arg0, sourceLoc); }

#define DEFINE_LOG_FUNCTION_SUBHELPER0(NAME) \
    inline void NAME (const Category& category, const std::string& format, const std::source_location& sourceLoc = std::source_location::current()) { cformattedLog<NAME ## _log>(category, sourceLoc, format, ""); } \
    inline void NAME (const std::string& format, const std::source_location& sourceLoc = std::source_location::current()) { NAME(defaultCategory, format, sourceLoc); }

#define DEFINE_LOG_FUNCTION_HELPER(name) \
    DEFINE_LOG_FUNCTION_SUBHELPER6(name) \
    DEFINE_LOG_FUNCTION_SUBHELPER5(name) \
    DEFINE_LOG_FUNCTION_SUBHELPER4(name) \
    DEFINE_LOG_FUNCTION_SUBHELPER3(name) \
    DEFINE_LOG_FUNCTION_SUBHELPER2(name) \
    DEFINE_LOG_FUNCTION_SUBHELPER1(name) \
    DEFINE_LOG_FUNCTION_SUBHELPER0(name)

    DEFINE_LOG_FUNCTION_HELPER(debug)
    DEFINE_LOG_FUNCTION_HELPER(info)
    DEFINE_LOG_FUNCTION_HELPER(warn)
    DEFINE_LOG_FUNCTION_HELPER(error)
}