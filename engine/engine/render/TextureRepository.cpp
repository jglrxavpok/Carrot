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
        // TODO: use history length
        auto it = textures[frameNumber % MAX_FRAMES_IN_FLIGHT].find(id);
        verify(it != textures[frameNumber % MAX_FRAMES_IN_FLIGHT].end(), "Did not create texture correctly?");
        return it->second;
    }

    Texture& ResourceRepository::getOrCreateTexture(const FrameResource& id, u64 frameNumber, vk::ImageUsageFlags textureUsages, const vk::Extent2D& viewportSize) {
        if(textures.empty()) {
            textures.resize(MAX_FRAMES_IN_FLIGHT);
        }
        // TODO: use history length
        auto it = textures[frameNumber % MAX_FRAMES_IN_FLIGHT].find(id.rootID);
        if(it != textures[frameNumber % MAX_FRAMES_IN_FLIGHT].end()) {
            return *it->second;
        }
        return createTexture(id, frameNumber, textureUsages, viewportSize);
    }

    Texture& ResourceRepository::createTexture(const FrameResource& resource, size_t frameIndex, vk::ImageUsageFlags textureUsages, const vk::Extent2D& viewportSize) {
        verify(resource.type == ResourceType::StorageImage || resource.type == ResourceType::RenderTarget, Carrot::sprintf("Resource %s is not a texture", resource.name.c_str()));
        // TODO: aliasing
        if(textures.empty()) {
            textures.resize(MAX_FRAMES_IN_FLIGHT);
        }

        verify(resource.imageOrigin != Render::ImageOrigin::SurfaceSwapchain, "Not allowed for swapchain images");

        auto it = textures[frameIndex].find(resource.rootID);
        if(it == textures[frameIndex].end()) {
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
            Texture::Ref texture = std::make_shared<Texture>(driver,
                                                size,
                                                textureUsages,
                                                format
            );
            texture->name(resource.name + " (RenderGraph)");
            textures[frameIndex][resource.rootID] = std::move(texture);
        } else {
            throw std::runtime_error("Texture already exists");
        }

        return getTexture(resource, frameIndex);
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

    Carrot::BufferAllocation& ResourceRepository::BufferChain::get(std::size_t swapchainIndex) {
        return buffers[swapchainIndex % buffers.size()];
    }

    ResourceRepository::BufferChain& ResourceRepository::createBuffer(const FrameResource& texture, vk::BufferUsageFlags usages) {
        verify(texture.type == ResourceType::StorageBuffer, "Resource is not a buffer");
        verify(!buffers.contains(texture.id), "Buffer already exists");
        auto& ref = buffers[texture.rootID];

        auto historyLengthIter = bufferReuseHistoryLengths.find(texture.id);
        std::size_t historyLength = historyLengthIter == bufferReuseHistoryLengths.end() ? 0 : historyLengthIter->second;
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

    void ResourceRepository::setBufferReuseHistoryLength(const Carrot::UUID& id, std::size_t historyLength) {
        bufferReuseHistoryLengths[id] = historyLength;
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

        for (auto& repo : textures) {
            for(const auto& affectedTexture : affectedIDs) {
                repo.erase(affectedTexture);
            }
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
