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
    class TextureRepository {
    public:
        TextureRepository(VulkanDriver& driver): driver(driver) {}

    public:
        Texture& create(const FrameResource& texture, size_t swapchainIndex, vk::ImageUsageFlags textureUsages, const vk::Extent2D& viewportSize);
        Texture& get(const FrameResource& texture, size_t swapchainIndex);
        Texture& get(const Carrot::UUID& id, size_t swapchainIndex);
        Texture::Ref getRef(const FrameResource& texture, size_t swapchainIndex);
        Texture::Ref getRef(const Carrot::UUID& id, size_t swapchainIndex);
        Texture& getOrCreate(const FrameResource& id, size_t swapchainIndex, vk::ImageUsageFlags textureUsages, const vk::Extent2D& viewportSize);
        vk::ImageUsageFlags& getUsages(const Carrot::UUID& id);

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
        std::unordered_map<Carrot::UUID, Carrot::UUID> textureOwners;
        std::unordered_map<Carrot::UUID, vk::ImageUsageFlags> usages;

        VR::Session* vrSession = nullptr;

        friend class GraphBuilder;
    };
}


