//
// Created by jglrxavpok on 10/03/2022.
//

#include "RenderPacketContainer.h"
#include <engine/utils/Profiling.h>

namespace Carrot::Render {
    PacketContainer::PacketContainer() {}

    void PacketContainer::beginFrame() {
        ZoneScoped;
        renderPackets.beginFrame();
        pushConstants.beginFrame();

        {
            Async::LockGuard l{genericDataAccess};
            if (!genericDynamicStorage.empty()) {
                std::size_t additionalSize = 0;
                for (const auto& v: genericDynamicStorage) {
                    additionalSize += v.size();
                }
                genericStaticStorage.resize(genericStaticStorage.size() + additionalSize);
                genericDynamicStorage.clear();
            }
            genericCursor = 0;
        }
    }

    /// Makes a new RenderPacket. The returned reference is valid only for the current frame
    Packet& PacketContainer::make(Carrot::Render::PassEnum pass, Carrot::Render::Viewport* viewport, std::source_location location) {
        ZoneScoped;
        auto& r = renderPackets.allocate(*this, pass, location);
        r.viewport = viewport;
        return r;
    }

    Packet::PushConstant& PacketContainer::makePushConstant() {
        return pushConstants.allocate(*this);
    }

    std::span<std::uint8_t> PacketContainer::allocateGeneric(std::size_t size) {
        Async::LockGuard l { genericDataAccess };
        if(genericCursor + size <= genericStaticStorage.size()) {
            std::uint8_t* ptr = genericStaticStorage.data() + genericCursor;
            genericCursor += size;
            return { ptr, size };
        }
        genericDynamicStorage.emplace_back();
        auto& v = genericDynamicStorage.back();
        v.resize(size);
        return { v.data(), v.size() };
    }

    std::span<std::uint8_t> PacketContainer::copyGeneric(const std::span<std::uint8_t>& toCopy) {
        auto copy = allocateGeneric(toCopy.size());
        std::memcpy(copy.data(), toCopy.data(), toCopy.size());
        return copy;
    }

    void PacketContainer::deallocateGeneric(std::span<std::uint8_t>&& toDestroy) {
        // no op
    }
}