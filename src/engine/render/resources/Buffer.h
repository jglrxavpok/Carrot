//
// Created by jglrxavpok on 27/11/2020.
//

#pragma once

#include "engine/vulkan/includes.h"
#include <set>
#include <engine/vulkan/VulkanDriver.h>
#include <engine/vulkan/DebugNameable.h>
#include <engine/vulkan/DeviceAddressable.h>

namespace Carrot {
    class Engine;
    class BufferView;
    class ResourceAllocator;

    /// Abstraction over Vulkan buffers
    class Buffer: public DebugNameable, public DeviceAddressable, std::enable_shared_from_this<Buffer> {

    private:
        bool mapped = false;
        VulkanDriver& driver;
        uint64_t size;
        vk::UniqueBuffer vkBuffer{};
        vk::UniqueDeviceMemory memory{};

        /// Creates and allocates a buffer with the given parameters
        //explicit Buffer(Engine& engine, vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, std::set<uint32_t> families = {});

        friend class ResourceAllocator;
        friend class std::unique_ptr<Buffer>;
        friend class std::shared_ptr<Buffer>;
    public:
        /// TODO: PRIVATE ONLY
        explicit Buffer(VulkanDriver& driver, vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, std::set<uint32_t> families = {});
        const vk::Buffer& getVulkanBuffer() const;

        uint64_t getSize() const;

        /// Copies this buffer to 'other' as a transfer operation
        void copyTo(Buffer& other, vk::DeviceSize offset) const;

        /// Mmaps the buffer memory into the application memory space, and copies the data from 'data'. Unmaps the memory when finished.
        /// Only use for host-visible and host-coherent memory
        void directUpload(const void* data, vk::DeviceSize length, vk::DeviceSize offset = 0);

        /// Stage an upload to the GPU, and wait for it to finish.
        /// Requires device-local memory
        /// \tparam T
        /// \param data
        template<typename... T>
        void stageUpload(const vector<T>&... data);

        template<typename... T>
        void stageUploadWithOffsets(const pair<uint64_t, vector<T>>&... offsetDataPairs);

        template<typename T>
        void stageUploadWithOffset(uint64_t offset, const T* data);

        template<typename T>
        T* map();

        void unmap();

        ~Buffer();

        void setDebugNames(const string& name) override;

        vk::DeviceAddress getDeviceAddress() const override;

        // TODO: make const asap
        BufferView getWholeView();
    };
}

#include "Buffer.ipp"