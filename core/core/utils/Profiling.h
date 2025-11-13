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

namespace vk {
    class CommandBuffer;
}

namespace tracy {
    class VkCtx;
}

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

    /**
     * Scoped marker for gpu work
     */
    struct ScopedGPUMarker {
        explicit ScopedGPUMarker(vk::CommandBuffer& cmds, const char* msg, const glm::vec4& color = {1,0,0,1});
        ~ScopedGPUMarker();

    private:
        vk::CommandBuffer& cmds;
    };

    /**
     * Wraps a TracyVkCtx to be move-aware
     */
    struct TracyVulkanContext {
        explicit TracyVulkanContext();
        explicit TracyVulkanContext(tracy::VkCtx* ctx);

        TracyVulkanContext(TracyVulkanContext&& other);
        TracyVulkanContext& operator=(TracyVulkanContext&& other);

        TracyVulkanContext(const TracyVulkanContext& other) = delete;
        TracyVulkanContext& operator=(const TracyVulkanContext& other) = delete;

        ~TracyVulkanContext();

        operator tracy::VkCtx*() const;
        tracy::VkCtx* get() const;

    private:
        tracy::VkCtx* ctx = nullptr;
    };

#define ScopedMarkerNamed(markerText, markerName) ZoneScopedN(markerText); Carrot::Profiling::ScopedPixMarker markerName { markerText }
#define ScopedMarker(markerText) ScopedMarkerNamed(markerText, _)
#define GPUZoneColored(ctx /*tracy vkcontext*/, cmds /*command buffer*/, markerText, color) TracyVkZone(ctx, cmds, markerText); Carrot::Profiling::ScopedGPUMarker gpuMarker { cmds, markerText, (color) }
#define GPUZone(ctx /*tracy vkcontext*/, cmds /*command buffer*/, markerText) TracyVkZone(ctx, cmds, markerText); Carrot::Profiling::ScopedGPUMarker gpuMarker { cmds, markerText }

    void beginFrame();
    const std::unordered_map<std::string, std::chrono::duration<float>>& getLastFrameTimes();

}
