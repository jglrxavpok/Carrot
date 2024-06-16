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
     * Scoped PIX marker. Calls BeginEvent and EndEvent on scope boundaries
     */
    struct ScopedPixMarker {
        explicit ScopedPixMarker(const char* name, std::source_location where = std::source_location::current());
        ~ScopedPixMarker();
    };

#define ScopedMarkerNamed(markerText, markerName) ZoneScopedN(markerText); Profiling::ScopedPixMarker markerName { markerText }
#define ScopedMarker(markerText) ScopedMarkerNamed(markerText, _)

    void beginFrame();
    const std::unordered_map<std::string, std::chrono::duration<float>>& getLastFrameTimes();

}
