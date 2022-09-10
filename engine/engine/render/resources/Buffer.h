//
// Created by jglrxavpok on 27/11/2020.
//

#pragma once

#include "engine/vulkan/includes.h"
#include <set>
#include <engine/vulkan/VulkanDriver.h>
#include <engine/vulkan/DebugNameable.h>
#include <engine/vulkan/DeviceAddressable.h>
#include <engine/render/resources/DeviceMemory.h>
#include <core/async/ParallelMap.hpp>

namespace Carrot {
    class Engine;
    class BufferView;
    class ResourceAllocator;

    /// Abstraction over Vulkan buffers
    class Buffer: public DebugNameable, public DeviceAddressable, std::enable_shared_from_this<Buffer> {
    public:
        //! How many buffers are alive, and where?
        static Async::ParallelMap<vk::DeviceAddress, const Buffer*> BufferByStartAddress;

        /// TODO: PRIVATE ONLY
        explicit Buffer(VulkanDriver& driver, vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, std::set<uint32_t> families = {});
        const vk::Buffer& getVulkanBuffer() const;

        uint64_t getSize() const;

        /// Copies this buffer to 'other' as a transfer operation
        void copyTo(Buffer& other, vk::DeviceSize srcOffset, vk::DeviceSize dstOffset) const;

        /// Copies the content of this buffer to the given buffer, as a transfer operation.
        void copyTo(std::span<std::uint8_t> out, vk::DeviceSize offset) const;

        /// Mmaps the buffer memory into the application memory space, and copies the data from this buffer to 'out'. Unmaps the memory when finished.
        /// Only use for host-visible and host-coherent memory
        void directDownload(std::span<std::uint8_t> out, vk::DeviceSize offset);

        /// Mmaps the buffer memory into the application memory space, and copies the data from 'data'. Unmaps the memory when finished.
        /// Only use for host-visible and host-coherent memory
        void directUpload(const void* data, vk::DeviceSize length, vk::DeviceSize offset = 0);

        /// Stage an upload to the GPU, and wait for it to finish.
        /// Requires device-local memory
        /// \tparam T
        /// \param data
        template<typename... T>
        void stageUpload(const std::vector<T>&... data);

        template<typename... T>
        void stageUploadWithOffsets(const std::pair<std::uint64_t, std::vector<T>>&... offsetDataPairs);

        template<typename T>
        void stageUploadWithOffset(uint64_t offset, const T* data, std::size_t totalLength = sizeof(T));

        /// returns a vk::DescriptorBufferInfo that references this buffer
        [[nodiscard]] vk::DescriptorBufferInfo asBufferInfo() const;

        template<typename T>
        T* map();

        void unmap();

        void flushMappedMemory(vk::DeviceSize start, vk::DeviceSize length);

        //! Does not immediately destroy the buffer. The destruction is deferred via VulkanRenderer::deferDestroy, unless destroyNow() was called before.
        ~Buffer();

        void setDebugNames(const std::string& name) override;

        vk::DeviceAddress getDeviceAddress() const override;

        // TODO: make const asap
        BufferView getWholeView();

        bool isDeviceLocal() const {
            return deviceLocal;
        }

    public:
        void destroyNow();

    private:
        VulkanDriver& driver;
        bool deviceLocal = false;
        uint64_t size;
        void* mappedPtr = nullptr;
        vk::UniqueBuffer vkBuffer{};
        DeviceMemory memory{};
        vk::DeviceAddress deviceAddress;

        /// Creates and allocates a buffer with the given parameters
        //explicit Buffer(Engine& engine, vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, std::set<uint32_t> families = {});

        friend class ResourceAllocator;
        friend class std::unique_ptr<Buffer>;
        friend class std::shared_ptr<Buffer>;
    };
}

#include "Buffer.ipp"