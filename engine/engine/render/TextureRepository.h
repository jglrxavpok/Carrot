//
// Created by jglrxavpok on 10/07/2021.
//

#pragma once
#include <vector>
#include <unordered_map>
#include <core/containers/Vector.hpp>

#include "engine/render/resources/Texture.h"
#include "engine/render/RenderPassData.h"
#include "engine/render/resources/BufferAllocation.h"
#include "core/utils/UUID.h"

namespace Carrot::VR {
    class Session;
}

namespace Carrot::Render {
    class ResourceRepository {
    public:
        class BufferChain;

        ResourceRepository(VulkanDriver& driver): driver(driver) {}

    public:
        Texture& getTexture(const FrameResource& texture, u64 frameNumber);
        Texture& getSwapchainTexture(const FrameResource& texture, size_t swapchainIndex);
        Texture& getTexture(const Carrot::UUID& id, u64 frameNumber);
        Texture::Ref getTextureRef(const FrameResource& texture, u64 frameNumber);
        Texture::Ref getTextureRef(const Carrot::UUID& id, u64 frameNumber);
        Texture& getOrCreateTexture(const FrameResource& id, u64 frameNumber, vk::ImageUsageFlags textureUsages, const vk::Extent2D& viewportSize);
        vk::ImageUsageFlags& getTextureUsages(const Carrot::UUID& id);

        BufferChain& createBuffer(const FrameResource& texture, vk::BufferUsageFlags usages);
        BufferAllocation& getBuffer(const FrameResource& texture, u64 frameNumber);
        BufferAllocation& getBuffer(const Carrot::UUID& id, u64 frameNumber);
        BufferAllocation& getOrCreateBuffer(const FrameResource& id, u64 frameNumber, vk::BufferUsageFlags usages);
        vk::BufferUsageFlags& getBufferUsages(const Carrot::UUID& id);
        void setBufferReuseHistoryLength(const Carrot::UUID& id, std::size_t historyLength);

        /// Which render pass is the creator of the texture with the given ID? Throws if no texture corresponds. Returns Carrot::UUID::null() if no creator has been set.
        Carrot::UUID getCreatorID(const Carrot::UUID& id) const;

    public:
        /// Removes all textures which belong to this render pass ID.
        void removeBelongingTo(const Carrot::UUID& id);

    public:
        void setXRSession(VR::Session* session);

    private:
        Texture& createTexture(const FrameResource& texture, size_t frameIndex, vk::ImageUsageFlags textureUsages, const vk::Extent2D& viewportSize);

        /// Sets which render pass is the creator of the texture with the given ID
        void setCreatorID(const Carrot::UUID& resourceID, const Carrot::UUID& creatorID);

    public:
        /**
         * Represents the various versions of a buffer across time.
         * Actually multiple buffers in a trenchcoat
         */
        class BufferChain {
        public:
            BufferChain() = default;

            void resize(const FrameResource& buffer, vk::BufferUsageFlags usages, std::size_t historyLength);
            Carrot::BufferAllocation& get(std::size_t swapchainIndex);

            BufferChain(const BufferChain&) = delete;
            BufferChain& operator=(const BufferChain&) = delete;
            BufferChain(BufferChain&&) = default;
            BufferChain& operator=(BufferChain&&) = default;

        private:
            Vector<Carrot::BufferAllocation> buffers;
        };
    private:

        VulkanDriver& driver;
        std::vector<std::unordered_map<Carrot::UUID, Carrot::Render::Texture::Ref>> textures;
        std::unordered_map<Carrot::UUID, vk::ImageUsageFlags> textureUsages;
        std::unordered_map<Carrot::UUID, Carrot::UUID> resourceOwners;

        std::unordered_map<Carrot::UUID, BufferChain> buffers;
        std::unordered_map<Carrot::UUID, std::size_t> bufferReuseHistoryLengths; // assumed 0 if key is missing
        std::unordered_map<Carrot::UUID, vk::BufferUsageFlags> bufferUsages;

        VR::Session* vrSession = nullptr;

        friend class GraphBuilder;
    };
}


