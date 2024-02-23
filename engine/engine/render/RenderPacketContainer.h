//
// Created by jglrxavpok on 10/03/2022.
//

#pragma once

#include "RenderPacket.h"
#include <core/async/Locks.h>

namespace Carrot::Render {
    /// Container responsible for RenderPacket-related allocations
    ///  This also means that push constant, and instance storage are handled by this container.
    ///  The strategy is to hold two structures:
    ///     - a contiguous one: its size will grow each frame if the amount of memory used in the previous frame was higher than the amount used before
    ///     - a linked-list-like one: its role is to allow for allocations to continue even if the entire contiguous storage has been used
    ///  Designed to be thread-safe (but blocking!)
    class PacketContainer {
    private:
        template<typename StoredType>
        class Storage {
        public:
            explicit Storage() {}

            ~Storage() {
                deleteStorage();
            }

            /// Completely reset the storage as if it was brand new (ie 0 elements)
            void reset() {
                Async::LockGuard l { lock };
                clearStorage();
                deleteStorage();
                dynamicStorage.resize(0);
            }

            /// Signals the start of a new frame. The storage is resized and its capacity is increased if necessary
            void beginFrame() {
                Async::LockGuard l { lock };
                clearStorage();
                if(!dynamicStorage.empty()) {
                    std::size_t previousSize = staticStorageSize;
                    deleteStorage();
                    staticStorage = static_cast<StoredType*>(malloc((previousSize + dynamicStorage.size()) * sizeof(StoredType)));
                    verify(staticStorage != nullptr, "allocation error!");
                    staticStorageSize = previousSize + dynamicStorage.size();
                }

                dynamicStorage.clear();
            }

            /// Allocates a 'StoredType' from this container. The reference is valid until the next call to 'beginFrame'
            template<typename... Args>
            StoredType& allocate(Args&&... args) {
                Async::LockGuard l { lock };
                if(cursor < staticStorageSize) {
                    auto ptr = new(&staticStorage[cursor++]) StoredType(std::forward<Args>(args)...);
                    return *ptr;
                }
                return dynamicStorage.emplace_back(std::forward<Args>(args)...);
            }
        private:
            void clearStorage() {
                if(staticStorage != nullptr) {
                    for (std::size_t i = 0; i < cursor; i++) {
                        staticStorage[i].~StoredType();
                    }
                }
                cursor = 0;
            }

            void deleteStorage() {
                if(staticStorage != nullptr) {
                    free(staticStorage);
                    staticStorage = nullptr;
                    staticStorageSize = 0;
                }
            }

        private:
            Async::SpinLock lock;
            std::size_t cursor = 0;
            StoredType* staticStorage = nullptr;
            std::size_t staticStorageSize = 0;
            std::list<StoredType> dynamicStorage;
        };

    public:
        explicit PacketContainer();

    public:
        /// Signals the start of a new frame. The storage is resized and its capacity is increased if necessary
        void beginFrame();

        /// Makes a new RenderPacket. The returned reference is valid only for the current frame
        Packet& make(Carrot::Render::PassEnum pass, const Carrot::Render::PacketType& packetType, Carrot::Render::Viewport* viewport, std::source_location location = std::source_location::current());

        Packet::PushConstant& makePushConstant();

        std::span<std::uint8_t> allocateGeneric(std::size_t size);
        std::span<std::uint8_t> copyGeneric(const std::span<std::uint8_t>& toCopy);
        void deallocateGeneric(std::span<std::uint8_t>&& toDestroy);

    private:
        Storage<Render::Packet> renderPackets;
        Storage<Render::Packet::PushConstant> pushConstants;

        Async::SpinLock genericDataAccess;
        std::size_t genericCursor = 0;
        std::vector<std::uint8_t> genericStaticStorage;
        std::list<std::vector<std::uint8_t>> genericDynamicStorage;
    };
}
