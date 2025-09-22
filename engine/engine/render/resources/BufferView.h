//
// Created by jglrxavpok on 10/03/2021.
//

#pragma once

#include <memory>
#include <source_location>
#include <span>
#include <core/async/ParallelMap.hpp>
#include <core/data/Hashes.h>
#include <engine/vulkan/DebugNameable.h>
#include <engine/vulkan/DeviceAddressable.h>

namespace Carrot {
    struct BufferViewDebugKey {
        vk::DeviceAddress start;
        vk::DeviceSize size;

        bool operator==(const BufferViewDebugKey &) const = default;
        bool operator!=(const BufferViewDebugKey &) const = default;
    };
}

template<>
struct std::hash<Carrot::BufferViewDebugKey> {
    std::size_t operator()(const Carrot::BufferViewDebugKey& k) const noexcept {
        std::size_t h = 0;
        Carrot::hash_combine(h, static_cast<std::size_t>(k.start));
        Carrot::hash_combine(h, static_cast<std::size_t>(k.size));
        return h;
    }
};

namespace Carrot {
    class ResourceAllocator;
    class Buffer;

    /// Represents a view inside a Carrot::Buffer.
    /// This does NOT own the data (like std::span)
    class BufferView: public DeviceAddressable {
    public:
        static Async::ParallelMap<BufferViewDebugKey, std::string> NameByRange;

        explicit BufferView(Carrot::ResourceAllocator* allocator, Carrot::Buffer& buffer, vk::DeviceSize start, vk::DeviceSize size);
        explicit BufferView(): buffer(nullptr), start(0), size(0), allocator(nullptr) {};
        BufferView(const BufferView&) = default;

        bool operator==(const BufferView&) const;
        BufferView& operator=(const BufferView&) = default;

        [[nodiscard]] vk::DeviceSize getStart() const { return start; };
        [[nodiscard]] vk::DeviceSize getSize() const { return size; };

        [[nodiscard]] Buffer& getBuffer() { return *buffer; };
        [[nodiscard]] const Buffer& getBuffer() const { return *buffer; };
        [[nodiscard]] const vk::Buffer& getVulkanBuffer() const;

        /// Returns a new view corresponding to this[start...]
        [[nodiscard]] BufferView subView(std::size_t start) const;
        /// Returns a new view corresponding to this[start..start+count[
        [[nodiscard]] BufferView subView(std::size_t start, std::size_t count) const;

        /// Calls subView(std::size_t) by computing the offset from this view's starting address
        [[nodiscard]] BufferView subViewFromAddress(vk::DeviceAddress start) const;
        /// Calls subView(std::size_t, std::size_t) by computing the offset from this view's starting address
        [[nodiscard]] BufferView subViewFromAddress(vk::DeviceAddress start, std::size_t count) const;

        /// Mmaps the buffer memory into the application memory space, and copies the data from 'data'. Unmaps the memory when finished.
        /// Only use for host-visible and host-coherent memory
        void directUpload(const void* data, vk::DeviceSize length, vk::DeviceSize offset = 0);

        /// Upload to device-local memory
        void stageUpload(const void* data, vk::DeviceSize length, vk::DeviceSize offset = 0);

        /// Upload to device-local memory, but signal the given semaphore once upload is finished
        void stageUpload(vk::Semaphore semaphore, const void* data, vk::DeviceSize length, vk::DeviceSize offset = 0);

        /**
         * Copies the input data into a temporary buffer, and copies that temporary buffer to GPU memory.
         * Note that this function does NOT wait for the copy to finish.
         * The GPU will "automatically" (this is handled by the engine) wait for the copy to finish before starting to render the current frame.
         *
         * @param data list of contiguous elements to copy to this GPU buffer
         * @param length length, in bytes, of the contents of 'data'
         * @param offset offset, in bytes, of where to copy data inside this buffer view
         */
        void uploadForFrame(const void* data, vk::DeviceSize length, vk::DeviceSize offset = 0, const std::source_location& sourceLocation = std::source_location::current());
        void uploadForFrameOnRenderThread(const void* data, vk::DeviceSize length, vk::DeviceSize offset = 0, const std::source_location& sourceLocation = std::source_location::current());

        void copyToAndWait(Carrot::BufferView destination) const;
        void copyTo(vk::Semaphore& signalSemaphore, Carrot::BufferView destination) const;
        void cmdCopyTo(vk::CommandBuffer& cmds, Carrot::BufferView destination) const;

        template<typename T>
        void directUpload(const std::span<const T>& data) {
            directUpload(data.data(), static_cast<vk::DeviceSize>(data.size() * sizeof(T)));
        }

        template<typename T>
        void stageUpload(const std::span<const T>& data) {
            stageUpload(data.data(), static_cast<vk::DeviceSize>(data.size() * sizeof(T)));
        }

        /**
         * Copies the input data into a temporary buffer, and copies that temporary buffer to GPU memory.
         * Note that this function does NOT wait for the copy to finish.
         * The GPU will "automatically" (this is handled by the engine) wait for the copy to finish before starting to render the current frame.
         *
         * @tparam T Data type to send
         * @param data list of contiguous elements to copy to this GPU buffer
         */
        template<typename T>
        void uploadForFrame(const std::span<const T>& data) {
            uploadForFrame(data.data(), static_cast<vk::DeviceSize>(data.size() * sizeof(T)));
        }

        template<typename T>
        void uploadForFrameOnRenderThread(const std::span<const T>& data) {
            uploadForFrameOnRenderThread(data.data(), static_cast<vk::DeviceSize>(data.size() * sizeof(T)));
        }

        /// Copies the contents of this buffer to the given memory
        void download(const std::span<std::uint8_t>& data, std::uint32_t offset = 0) const;

        [[nodiscard]] vk::DescriptorBufferInfo asBufferInfo() const;

        explicit operator bool() const {
            return !!buffer;
        }

        void flushMappedMemory();

        template<typename T>
        T* map();

        void* mapGeneric();

        void unmap();

        vk::DeviceAddress getDeviceAddress() const override;

    public:
        ~BufferView();

    private:
        Carrot::ResourceAllocator* allocator = nullptr; // TODO: remove allocator ref, maybe make BufferRef somewhere. Need to do a pass on all usages of BufferView/Buffer
        Carrot::Buffer* buffer;
        vk::DeviceSize start;
        vk::DeviceSize size;
    };

}

#include "BufferView.ipp"
