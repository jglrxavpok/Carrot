//
// Created by jglrxavpok on 19/08/2022.
//

#pragma once

#include <atomic>
#include <unordered_map>
#include <source_location>
#include <core/data/Hashes.h>
#include <core/async/ParallelMap.hpp>
#include "engine/vulkan/includes.h"
#include "engine/vulkan/DebugNameable.h"

namespace Carrot {
    //! RAII construct for memory allocated on the graphics device (GPU).
    //! Not copyable, but move-able
    class DeviceMemory: public Carrot::DebugNameable {
    public:
        //! How much memory has been allocated in total?
        static std::atomic<std::size_t> TotalMemoryUsed;

        //! How is memory balanced between allocation sources?
        static Async::ParallelMap<std::string, std::size_t> MemoryUsedByLocation;

        //! No memory associated
        DeviceMemory(std::source_location sourceLocation = std::source_location::current());

        //! Allocates the given memory. Can throw if allocation fails
        DeviceMemory(const vk::MemoryAllocateInfo& allocInfo, std::source_location sourceLocation = std::source_location::current());

        DeviceMemory(DeviceMemory&&);
        DeviceMemory& operator=(DeviceMemory&&);

        //! Destroy this object and frees the memory
        ~DeviceMemory();

    public: // non copyable
        DeviceMemory(const DeviceMemory&) = delete;
        DeviceMemory& operator=(const DeviceMemory&) = delete;

    public:
        //! Corresponding Vulkan memory. Can be VK_NULL_HANDLE
        vk::DeviceMemory getVulkanMemory() const { return vkMemory; }
        vk::DeviceSize getSize() const { return size; }

    protected:
        void setDebugNames(const std::string& name) override;

    private:
        void free();

    private:
        vk::DeviceMemory vkMemory = VK_NULL_HANDLE;
        std::string sourceLocation;
        vk::DeviceSize size = 0;
    };
}
