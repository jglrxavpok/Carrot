//
// Created by jglrxavpok on 10/07/2021.
//

#include "TextureRepository.h"
#include "core/utils/Assert.h"
#include "engine/utils/Macros.h"
#include "engine/task/TaskScheduler.h"

#include "engine/vr/Session.h"

namespace Carrot::Render {
    Texture& TextureRepository::get(const FrameResource& texture, size_t swapchainIndex) {
        return *getRef(texture, swapchainIndex);
    }

    Texture& TextureRepository::get(const Carrot::UUID& id, size_t swapchainIndex) {
        return *getRef(id, swapchainIndex);
    }

    Texture::Ref TextureRepository::getRef(const FrameResource& texture, size_t swapchainIndex) {
        if(texture.imageOrigin == Render::ImageOrigin::SurfaceSwapchain) {
            verify(texture.pOriginWindow != nullptr, "Cannot get the swapchain texture if there is no origin window!");
            return texture.pOriginWindow->getSwapchainTexture(swapchainIndex);
        }
        return getRef(texture.rootID, swapchainIndex);
    }

    Texture::Ref TextureRepository::getRef(const Carrot::UUID& id, size_t swapchainIndex) {
        auto it = textures[swapchainIndex].find(id);
        verify(it != textures[swapchainIndex].end(), "Did not create texture correctly?");
        return it->second;
    }

    Texture& TextureRepository::getOrCreate(const FrameResource& id, size_t swapchainIndex, vk::ImageUsageFlags textureUsages, const vk::Extent2D& viewportSize) {
        if(textures.empty()) {
            textures.resize(driver.getSwapchainImageCount());
        }
        auto it = textures[swapchainIndex].find(id.rootID);
        if(it != textures[swapchainIndex].end()) {
            return *it->second;
        }
        return create(id, swapchainIndex, textureUsages, viewportSize);
    }

    Texture& TextureRepository::create(const FrameResource& resource, size_t frameIndex, vk::ImageUsageFlags textureUsages, const vk::Extent2D& viewportSize) {
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
            texture->name(resource.name);
            textures[frameIndex][resource.rootID] = std::move(texture);
        } else {
            throw std::runtime_error("Texture already exists");
        }

        return get(resource, frameIndex);
    }

    vk::ImageUsageFlags& TextureRepository::getUsages(const UUID& id) {
        return usages[id];
    }

    void TextureRepository::setCreatorID(const Carrot::UUID& resourceID, const Carrot::UUID& creatorID) {
        verify(textureOwners.find(resourceID) == textureOwners.end(), "There is already an other for this texture");
        textureOwners[resourceID] = creatorID;
    }

    Carrot::UUID TextureRepository::getCreatorID(const Carrot::UUID& id) const {
        auto it = textureOwners.find(id);
        if(it == textureOwners.end()) {
            return Carrot::UUID::null();
        }
        return it->second;
    }

    void TextureRepository::removeBelongingTo(const Carrot::UUID& id) {
        std::vector<Carrot::UUID> affectedTextures;
        for(const auto& [textureID, ownerID] : textureOwners) {
            if(ownerID == id) {
                affectedTextures.push_back(textureID);
            }
        }

        for (auto& repo : textures) {
            for(const auto& affectedTexture : affectedTextures) {
                repo.erase(affectedTexture);
            }
        }
    }

    void TextureRepository::setXRSession(VR::Session *session) {
        assert(session != nullptr);
        vrSession = session;
    }
}
