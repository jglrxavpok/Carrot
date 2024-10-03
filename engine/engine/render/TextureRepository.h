//
// Created by jglrxavpok on 10/07/2021.
//

#pragma once
#include <vector>
#include <unordered_map>
#include "engine/render/resources/Texture.h"
#include "engine/render/RenderPassData.h"
#include "core/utils/UUID.h"

namespace Carrot::VR {
    class Session;
}

namespace Carrot::Render {
    class ResourceRepository {
    public:
        ResourceRepository(VulkanDriver& driver): driver(driver) {}

    public:
        Texture& createTexture(const FrameResource& texture, size_t swapchainIndex, vk::ImageUsageFlags textureUsages, const vk::Extent2D& viewportSize);
        Texture& getTexture(const FrameResource& texture, size_t swapchainIndex);
        Texture& getTexture(const Carrot::UUID& id, size_t swapchainIndex);
        Texture::Ref getTextureRef(const FrameResource& texture, size_t swapchainIndex);
        Texture::Ref getTextureRef(const Carrot::UUID& id, size_t swapchainIndex);
        Texture& getOrCreateTexture(const FrameResource& id, size_t swapchainIndex, vk::ImageUsageFlags textureUsages, const vk::Extent2D& viewportSize);
        vk::ImageUsageFlags& getTextureUsages(const Carrot::UUID& id);

        BufferAllocation& createBuffer(const FrameResource& texture, size_t swapchainIndex, vk::BufferUsageFlags usages);
        BufferAllocation& getBuffer(const FrameResource& texture, size_t swapchainIndex);
        BufferAllocation& getBuffer(const Carrot::UUID& id, size_t swapchainIndex);
        BufferAllocation& getOrCreateBuffer(const FrameResource& id, size_t swapchainIndex, vk::BufferUsageFlags usages);
        vk::BufferUsageFlags& getBufferUsages(const Carrot::UUID& id);

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
        VulkanDriver& driver;
        std::vector<std::unordered_map<Carrot::UUID, Carrot::Render::Texture::Ref>> textures;
        std::unordered_map<Carrot::UUID, Carrot::BufferAllocation> buffers;
        std::unordered_map<Carrot::UUID, Carrot::UUID> resourceOwners;
        std::unordered_map<Carrot::UUID, vk::ImageUsageFlags> textureUsages;
        std::unordered_map<Carrot::UUID, vk::BufferUsageFlags> bufferUsages;

        VR::Session* vrSession = nullptr;

        friend class GraphBuilder;
    };
}


