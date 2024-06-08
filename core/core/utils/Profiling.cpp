//
// Created by jglrxavpok on 08/11/2021.
//

#include "Profiling.h"
#include "core/io/Logging.hpp"
#include "core/async/Locks.h"
#include <unordered_map>
#include <tracy/TracyC.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <WinPixEventRuntime/pix3.h>
#endif

namespace Carrot::Profiling {
    static Carrot::Log::Category category { "Profiler" };
    static Async::SpinLock timesAccess;
    static std::unordered_map<std::string, std::chrono::duration<float>> times{};
    static std::unordered_map<std::string, std::chrono::duration<float>> lastFrameTimes{};

    void beginFrame() {
        Async::LockGuard lk { timesAccess };
        lastFrameTimes = std::move(times);
        times.clear();
    }

    const std::unordered_map<std::string, std::chrono::duration<float>>& getLastFrameTimes() {
        return lastFrameTimes;
    }

    ScopedTimer::ScopedTimer(std::string_view name): name(name) {
        start = std::chrono::steady_clock::now();
    }

    float ScopedTimer::getTime() const {
        return duration_cast<std::chrono::duration<float>>((std::chrono::steady_clock::now() - start)).count();
    }

    ScopedTimer::~ScopedTimer() {
        Async::LockGuard lk { timesAccess };
        auto elapsed = duration_cast<std::chrono::duration<float>>((std::chrono::steady_clock::now() - start));
        auto strcpy = name;
        times[strcpy] += elapsed;
    }

    PrintingScopedTimer::~PrintingScopedTimer() {
        auto elapsed = duration_cast<std::chrono::duration<float>>((std::chrono::steady_clock::now() - start));
        Carrot::Log::debug(category, "'%s' took %f", name.c_str(), elapsed.count());
    }


    ScopedMarker::ScopedMarker(const char* name, std::source_location where) {
        TracyCZoneN(tracyZone, name, true);
        currentTracyZone = tracyZone;
        PIXBeginEvent(PIX_COLOR_INDEX(reinterpret_cast<std::uint64_t>(name) % 10), name);
    }

    ScopedMarker::~ScopedMarker() {
        PIXEndEvent();
        TracyCZoneEnd(currentTracyZone);
    }


}
