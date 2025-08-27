//
// Created by jglrxavpok on 10/07/2021.
//

#include "TextureRepository.h"

#include <engine/vulkan/VulkanDriver.h>

#include "core/utils/Assert.h"
#include "engine/utils/Macros.h"
#include "engine/task/TaskScheduler.h"

#include "engine/vr/Session.h"
#include "resources/ResourceAllocator.h"

namespace Carrot::Render {
    Texture& ResourceRepository::getSwapchainTexture(const FrameResource& resource, size_t swapchainIndex) {
        verify(resource.imageOrigin == Render::ImageOrigin::SurfaceSwapchain, "Texture is not from a swapchain!");
        verify(resource.pOriginWindow != nullptr, "Cannot get the swapchain texture if there is no origin window!");
        return *(resource.pOriginWindow->getSwapchainTexture(swapchainIndex));
    }

    Texture& ResourceRepository::getTexture(const FrameResource& texture, u64 frameNumber) {
        return *getTextureRef(texture, frameNumber);
    }

    Texture& ResourceRepository::getTexture(const Carrot::UUID& id, u64 frameNumber) {
        return *getTextureRef(id, frameNumber);
    }

    Texture::Ref ResourceRepository::getTextureRef(const FrameResource& texture, u64 frameNumber) {
        verify(texture.imageOrigin != Render::ImageOrigin::SurfaceSwapchain, "Not allowed for swapchain images");
        return getTextureRef(texture.rootID, frameNumber);
    }

    Texture::Ref ResourceRepository::getTextureRef(const Carrot::UUID& id, u64 frameNumber) {
        return textures.at(id).get(frameNumber);
    }

    Texture& ResourceRepository::getOrCreateTexture(const FrameResource& id, u64 frameNumber, vk::ImageUsageFlags textureUsages, const vk::Extent2D& viewportSize) {
        auto it = textures.find(id.rootID);
        if (it != textures.end()) {
            return *it->second.get(frameNumber);
        }
        return createTexture(id, frameNumber, textureUsages, viewportSize);
    }

    Texture& ResourceRepository::createTexture(const FrameResource& resource, size_t frameIndex, vk::ImageUsageFlags textureUsages, const vk::Extent2D& viewportSize) {
        verify(resource.type == ResourceType::StorageImage || resource.type == ResourceType::RenderTarget, Carrot::sprintf("Resource %s is not a texture", resource.name.c_str()));

        // TODO: aliasing?

        verify(resource.imageOrigin != Render::ImageOrigin::SurfaceSwapchain, "Not allowed for swapchain images");
        verify(!textures.contains(resource.rootID), "Texture already exists!");
        auto& chain = textures[resource.rootID];

        auto historyLengthIter = resourceReuseHistoryLengths.find(resource.rootID);
        std::size_t historyLength = historyLengthIter == resourceReuseHistoryLengths.end() ? 0 : historyLengthIter->second;
        chain.resize(resource, viewportSize, textureUsages, historyLength);

        return *chain.get(frameIndex);
    }

    static BufferAllocation makeBufferAlloc(const FrameResource& buffer, vk::BufferUsageFlags usages) {
        BufferAllocation alloc = GetResourceAllocator().allocateDeviceBuffer(buffer.bufferSize, usages);
        alloc.name(buffer.name);
        return alloc;
    }

    void ResourceRepository::BufferChain::resize(const FrameResource& buffer, vk::BufferUsageFlags usages, std::size_t historyLength) {
        std::size_t bufferCount = historyLength+1;
        buffers.resize(bufferCount);
        for (int i = 0; i < bufferCount; ++i) {
            buffers[i] = std::move(makeBufferAlloc(buffer, usages));
        }
    }

    void ResourceRepository::TextureChain::resize(const FrameResource& resource, const vk::Extent2D& viewportSize, vk::ImageUsageFlags usages, std::size_t historyLength) {
        std::size_t bufferCount = historyLength+1;
        textures.resize(bufferCount);
        for (int i = 0; i < bufferCount; ++i) {
            vk::Extent3D size;
            switch(resource.size.type) {
                case TextureSize::Type::SwapchainProportional: {
                    size.width = static_cast<std::uint32_t>(resource.size.width * viewportSize.width);
                    size.height = static_cast<std::uint32_t>(resource.size.height * viewportSize.height);
                    size.depth = static_cast<std::uint32_t>(resource.size.depth * 1);
                } break;

                case TextureSize::Type::Fixed: {
                    size.width = static_cast<std::uint32_t>(resource.size.width);
                    size.height = static_cast<std::uint32_t>(resource.size.height);
                    size.depth = static_cast<std::uint32_t>(resource.size.depth);
                } break;
            }
            auto format = resource.format;

            verify(resource.imageOrigin == Render::ImageOrigin::Created, "Must be an explicitely created texture");
            textures[i] = std::make_shared<Texture>(GetVulkanDriver(),
                                                size,
                                                usages,
                                                format
            );
            textures[i]->name(resource.name + " (RenderGraph)");
        }
    }

    Carrot::BufferAllocation& ResourceRepository::BufferChain::get(u64 frameNumber) {
        return buffers[frameNumber % buffers.size()];
    }

    Carrot::Render::Texture::Ref& ResourceRepository::TextureChain::get(u64 frameNumber) {
        return textures[frameNumber % textures.size()];
    }

    i64 ResourceRepository::TextureChain::size() const {
        return textures.size();
    }


    ResourceRepository::BufferChain& ResourceRepository::createBuffer(const FrameResource& texture, vk::BufferUsageFlags usages) {
        verify(texture.type == ResourceType::StorageBuffer, "Resource is not a buffer");
        verify(!buffers.contains(texture.id), "Buffer already exists");
        auto& ref = buffers[texture.rootID];

        auto historyLengthIter = resourceReuseHistoryLengths.find(texture.id);
        std::size_t historyLength = historyLengthIter == resourceReuseHistoryLengths.end() ? 0 : historyLengthIter->second;
        ref.resize(texture, usages, historyLength);
        return ref;
    }

    BufferAllocation& ResourceRepository::getBuffer(const FrameResource& texture, u64 frameNumber) {
        return getBuffer(texture.rootID, frameNumber);
    }

    BufferAllocation& ResourceRepository::getBuffer(const Carrot::UUID& id, u64 frameNumber) {
        return buffers.at(id).get(frameNumber);
    }

    BufferAllocation& ResourceRepository::getOrCreateBuffer(const FrameResource& id, u64 frameNumber, vk::BufferUsageFlags usages) {
        if(!buffers.contains(id.rootID)) {
            createBuffer(id, usages);
        }
        return buffers[id.rootID].get(frameNumber);
    }

    vk::ImageUsageFlags& ResourceRepository::getTextureUsages(const UUID& id) {
        return textureUsages[id];
    }

    vk::BufferUsageFlags& ResourceRepository::getBufferUsages(const UUID& id) {
        return bufferUsages[id];
    }

    void ResourceRepository::setResourceReuseHistoryLength(const Carrot::UUID& id, std::size_t historyLength) {
        resourceReuseHistoryLengths[id] = historyLength;
    }

    void ResourceRepository::setCreatorID(const Carrot::UUID& resourceID, const Carrot::UUID& creatorID) {
        verify(resourceOwners.find(resourceID) == resourceOwners.end(), "There is already an owner for this texture");
        resourceOwners[resourceID] = creatorID;
    }

    Carrot::UUID ResourceRepository::getCreatorID(const Carrot::UUID& id) const {
        auto it = resourceOwners.find(id);
        if(it == resourceOwners.end()) {
            return Carrot::UUID::null();
        }
        return it->second;
    }

    void ResourceRepository::removeBelongingTo(const Carrot::UUID& id) {
        std::vector<Carrot::UUID> affectedIDs;
        for(const auto& [textureID, ownerID] : resourceOwners) {
            if(ownerID == id) {
                affectedIDs.push_back(textureID);
            }
        }

        for(const auto& affectedTexture : affectedIDs) {
            textures.erase(affectedTexture);
        }
        for(const auto& affectedTexture : affectedIDs) {
            buffers.erase(affectedTexture);
        }
    }

    void ResourceRepository::setXRSession(VR::Session *session) {
        assert(session != nullptr);
        vrSession = session;
    }
}
