//
// Created by jglrxavpok on 27/11/2020.
//

#pragma once

#include <Engine.h>
#include "vulkan/includes.h"
#include <set>

namespace Carrot {
    /// Abstraction over Vulkan buffers
    class Buffer {

    private:
        Engine& engine;
        uint64_t size;
        vk::UniqueBuffer vkBuffer{};
        vk::UniqueDeviceMemory memory{};

    public:
        explicit Buffer(Engine& engine, vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, std::set<uint32_t> families = {});

        const vk::Buffer& getVulkanBuffer() const;

        void copyTo(Buffer& other) const;

        /// Stage an upload to the GPU
        /// \tparam T
        /// \param data
        template<typename T>
        void stageUpload(const std::vector<T>& data);
    };
}

#include "render/Buffer.ipp"