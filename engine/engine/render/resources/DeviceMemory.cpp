//
// Created by jglrxavpok on 19/08/2022.
//

#include "DeviceMemory.h"
#include "engine/utils/Macros.h"
#include "engine/Engine.h"
#include <core/io/Logging.hpp>
#include <engine/console/RuntimeOption.hpp>

static constexpr bool VerboseOutput = false;
static Carrot::RuntimeOption LogGPUAllocations{"Debug/Log GPU Allocations", true};
static Carrot::RuntimeOption LogGPUAllocationsWithCallstack{"Debug/Log GPU Allocation callstacks", false};

namespace Carrot {
    std::atomic<std::size_t> DeviceMemory::TotalMemoryUsed = 0;
    Async::ParallelMap<std::string, std::size_t> DeviceMemory::MemoryUsedByLocation;

    DeviceMemory::DeviceMemory(std::source_location location) {
        sourceLocation = Carrot::sprintf("%s::%s %llu", location.file_name(), location.function_name(), (std::uint64_t)location.line());
    }

    DeviceMemory::DeviceMemory(const vk::MemoryAllocateInfo& allocInfo, std::source_location location) {
        sourceLocation = Carrot::sprintf("%s::%s %llu", location.file_name(), location.function_name(), (std::uint64_t)location.line());

        if constexpr(VerboseOutput) {
            Carrot::Log::debug("Allocating at %s: %lu (Current usage before alloc: %llu)", sourceLocation.c_str(), (std::uint64_t)allocInfo.allocationSize, TotalMemoryUsed.load());
        }

        size = allocInfo.allocationSize;
        vkMemory = GetVulkanDriver().getLogicalDevice().allocateMemory(allocInfo, GetEngine().getAllocator());
        if (LogGPUAllocations) {
            if (LogGPUAllocationsWithCallstack) {
                TracyAllocNS(vkMemory, size, 20, "GPU");
            } else {
                TracyAllocN(vkMemory, size, "GPU");
            }
        }

        TotalMemoryUsed += size;
        MemoryUsedByLocation.getOrCompute(sourceLocation, []() { return 0; }) += size;
    }

    DeviceMemory::~DeviceMemory() {
        free();
    }

    DeviceMemory::DeviceMemory(DeviceMemory&& other) {
        *this = std::move(other);
    }

    DeviceMemory& DeviceMemory::operator=(DeviceMemory&& other) {
        free();
        vkMemory = other.vkMemory;
        size = other.size;
        sourceLocation = other.sourceLocation;
        other.vkMemory = nullptr;
        other.size = 0;
        auto location = std::source_location::current();
        other.sourceLocation = Carrot::sprintf("%s::%s %llu", location.file_name(), location.function_name(), (std::uint64_t)location.line());
        return *this;
    }

    void DeviceMemory::setDebugNames(const std::string& name) {
        if(vkMemory) {
            nameSingle<vk::DeviceMemory>(name, vkMemory);
        }
    }

    void DeviceMemory::free() {
        if(vkMemory) {
            if (LogGPUAllocations) {
                if (LogGPUAllocationsWithCallstack) {
                    TracyFreeNS(vkMemory, 20, "GPU");
                } else {
                    TracyFreeN(vkMemory, "GPU");
                }
            }
            GetEngine().getLogicalDevice().freeMemory(vkMemory, GetEngine().getAllocator());
            vkMemory = nullptr;
        }

        if(size > 0) {
            if constexpr(VerboseOutput) {
                Carrot::Log::debug("Freeing of %s %llu", sourceLocation.c_str(), (std::uint64_t) size);
            }
            TotalMemoryUsed -= size;
            MemoryUsedByLocation.getOrCompute(sourceLocation, []() { return 0; }) -= size;
        }
    }
}