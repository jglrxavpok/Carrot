//
// Created by jglrxavpok on 08/11/2021.
//

#pragma once

#include <tracy/Tracy.hpp>
#include <client/TracyProfiler.hpp>
#include <chrono>
#include <source_location>
#include <unordered_map>
#include <string>
#include <string_view>
#include <tracy/TracyC.h>

namespace Carrot::Profiling {
    class ScopedTimer {
    public:
        explicit ScopedTimer(std::string_view name);
        virtual ~ScopedTimer();

        float getTime() const;

    protected:
        std::string name;
        std::chrono::steady_clock::time_point start;
    };

    class PrintingScopedTimer: public ScopedTimer {
    public:
        explicit PrintingScopedTimer(std::string_view name): ScopedTimer::ScopedTimer(name) {};
        virtual ~PrintingScopedTimer() override;
    };

    /**
     * Scoped marker.
     * Will send data to PIX and/or Tracy depending on configuration
     */
    class ScopedMarker {
    public:
        explicit ScopedMarker(const char* name, std::source_location where = std::source_location::current());
        ~ScopedMarker();

    private:
        TracyCZoneCtx currentTracyZone;
    };

    void beginFrame();
    const std::unordered_map<std::string, std::chrono::duration<float>>& getLastFrameTimes();

}
