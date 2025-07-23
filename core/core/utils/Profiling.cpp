//
// Created by jglrxavpok on 08/11/2021.
//

#include "Profiling.h"
#include "core/io/Logging.hpp"
#include "core/async/Locks.h"
#include <unordered_map>
#include <core/Macros.h>
#include <tracy/TracyC.h>

// Must match with engine/vulkan/includes.h
#undef VULKAN_HPP_DISABLE_ENHANCED_MODE
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <WinPixEventRuntime/pix3.h>
#else
#define PIXBeginEvent(color, name)
#define PIXEndEvent()
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


    ScopedPixMarker::ScopedPixMarker(const char* name, std::source_location where) {
        PIXBeginEvent(PIX_COLOR_INDEX(reinterpret_cast<std::uint64_t>(name) % 10), name);
    }

    ScopedPixMarker::~ScopedPixMarker() {
        PIXEndEvent();
    }

    ScopedGPUMarker::ScopedGPUMarker(vk::CommandBuffer& cmds, const char* msg, const glm::vec4& color): cmds(cmds) {
        vk::DebugUtilsLabelEXT labelInfo{};
        labelInfo.pLabelName = msg;
        labelInfo.color[0] = color[0];
        labelInfo.color[1] = color[1];
        labelInfo.color[2] = color[2];
        labelInfo.color[3] = color[3];
        cmds.beginDebugUtilsLabelEXT(labelInfo);
    }

    ScopedGPUMarker::~ScopedGPUMarker() {
        cmds.endDebugUtilsLabelEXT();
    }


}
