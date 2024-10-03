//
// Created by jglrxavpok on 10/07/2021.
//

#include "TextureRepository.h"
#include "core/utils/Assert.h"
#include "engine/utils/Macros.h"
#include "engine/task/TaskScheduler.h"

#include "engine/vr/Session.h"
#include "resources/ResourceAllocator.h"

namespace Carrot::Render {
    Texture& ResourceRepository::getTexture(const FrameResource& texture, size_t swapchainIndex) {
        return *getTextureRef(texture, swapchainIndex);
    }

    Texture& ResourceRepository::getTexture(const Carrot::UUID& id, size_t swapchainIndex) {
        return *getTextureRef(id, swapchainIndex);
    }

    Texture::Ref ResourceRepository::getTextureRef(const FrameResource& texture, size_t swapchainIndex) {
        if(texture.imageOrigin == Render::ImageOrigin::SurfaceSwapchain) {
            verify(texture.pOriginWindow != nullptr, "Cannot get the swapchain texture if there is no origin window!");
            return texture.pOriginWindow->getSwapchainTexture(swapchainIndex);
        }
        return getTextureRef(texture.rootID, swapchainIndex);
    }

    Texture::Ref ResourceRepository::getTextureRef(const Carrot::UUID& id, size_t swapchainIndex) {
        auto it = textures[swapchainIndex].find(id);
        verify(it != textures[swapchainIndex].end(), "Did not create texture correctly?");
        return it->second;
    }

    Texture& ResourceRepository::getOrCreateTexture(const FrameResource& id, size_t swapchainIndex, vk::ImageUsageFlags textureUsages, const vk::Extent2D& viewportSize) {
        if(textures.empty()) {
            textures.resize(driver.getSwapchainImageCount());
        }
        auto it = textures[swapchainIndex].find(id.rootID);
        if(it != textures[swapchainIndex].end()) {
            return *it->second;
        }
        return createTexture(id, swapchainIndex, textureUsages, viewportSize);
    }

    Texture& ResourceRepository::createTexture(const FrameResource& resource, size_t frameIndex, vk::ImageUsageFlags textureUsages, const vk::Extent2D& viewportSize) {
        verify(resource.type == ResourceType::StorageImage || resource.type == ResourceType::RenderTarget, Carrot::sprintf("Resource %s is not a texture", resource.name.c_str()));
        // TODO: aliasing
        if(textures.empty()) {
            textures.resize(driver.getSwapchainImageCount());
        }

        if(resource.imageOrigin == Render::ImageOrigin::SurfaceSwapchain) {
            verify(resource.pOriginWindow != nullptr, "Cannot get the swapchain texture if there is no origin window!");
            return *(resource.pOriginWindow->getSwapchainTexture(frameIndex));
        }

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

    BufferAllocation& ResourceRepository::createBuffer(const FrameResource& texture, size_t swapchainIndex, vk::BufferUsageFlags usages) {
        verify(texture.type == ResourceType::StorageBuffer, "Resource is not a buffer");
        verify(buffers.contains(texture.id), "Buffer already exists");
        auto& ref = buffers[texture.rootID];
        ref = makeBufferAlloc(texture, usages);
        return ref;
    }

    BufferAllocation& ResourceRepository::getBuffer(const FrameResource& texture, size_t swapchainIndex) {
        return getBuffer(texture.rootID, swapchainIndex);
    }

    BufferAllocation& ResourceRepository::getBuffer(const Carrot::UUID& id, size_t swapchainIndex) {
        return buffers.at(id);
    }

    BufferAllocation& ResourceRepository::getOrCreateBuffer(const FrameResource& id, size_t swapchainIndex, vk::BufferUsageFlags usages) {
        auto [iter, wasNew] = buffers.try_emplace(id.rootID);
        if(wasNew) {
            iter->second = makeBufferAlloc(id, usages);
        }
        return iter->second;
    }


    vk::ImageUsageFlags& ResourceRepository::getTextureUsages(const UUID& id) {
        return textureUsages[id];
    }

    vk::BufferUsageFlags& ResourceRepository::getBufferUsages(const UUID& id) {
        return bufferUsages[id];
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
