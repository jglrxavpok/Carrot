//
// Created by jglrxavpok on 10/07/2021.
//

#pragma once
#include <vector>
#include <unordered_map>
#include <core/containers/Vector.hpp>

#include "engine/render/resources/Texture.h"
#include "engine/render/RenderPassData.h"
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
        Texture& createTexture(const FrameResource& texture, size_t swapchainIndex, vk::ImageUsageFlags textureUsages, const vk::Extent2D& viewportSize);
        Texture& getTexture(const FrameResource& texture, size_t swapchainIndex);
        Texture& getTexture(const Carrot::UUID& id, size_t swapchainIndex);
        Texture::Ref getTextureRef(const FrameResource& texture, size_t swapchainIndex);
        Texture::Ref getTextureRef(const Carrot::UUID& id, size_t swapchainIndex);
        Texture& getOrCreateTexture(const FrameResource& id, size_t swapchainIndex, vk::ImageUsageFlags textureUsages, const vk::Extent2D& viewportSize);
        vk::ImageUsageFlags& getTextureUsages(const Carrot::UUID& id);

        BufferChain& createBuffer(const FrameResource& texture, vk::BufferUsageFlags usages);
        BufferAllocation& getBuffer(const FrameResource& texture, size_t frameIndex);
        BufferAllocation& getBuffer(const Carrot::UUID& id, size_t frameIndex);
        BufferAllocation& getOrCreateBuffer(const FrameResource& id, size_t frameIndex, vk::BufferUsageFlags usages);
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
        /// Sets which render pass is the creator of the texture with the given ID
        void setCreatorID(const Carrot::UUID& resourceID, const Carrot::UUID& creatorID);

    private:
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


