//
// Created by jglrxavpok on 08/11/2021.
//

#include "Profiling.h"
#include "core/io/Logging.hpp"
#include "core/async/Locks.h"
#include <unordered_map>

namespace Carrot::Profiling {
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
        Carrot::Log::debug("[Profiler] '%s' took %f", name.c_str(), elapsed.count());
    }
}