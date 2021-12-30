//
// Created by jglrxavpok on 10/07/2021.
//

#include "TextureRepository.h"
#include "core/utils/Assert.h"
#include "engine/utils/Macros.h"

#ifdef ENABLE_VR
#include "engine/vr/Session.h"
#endif

namespace Carrot::Render {
    Texture& TextureRepository::get(const FrameResource& texture, size_t swapchainIndex) {
        auto it = textures[swapchainIndex].find(texture.rootID);
        verify(it != textures[swapchainIndex].end(), "Did not create texture correctly?");
        return get(texture.rootID, swapchainIndex);
    }

    Texture& TextureRepository::get(const Carrot::UUID& id, size_t swapchainIndex) {
        auto it = textures[swapchainIndex].find(id);
        verify(it != textures[swapchainIndex].end(), "Did not create texture correctly?");
        return *it->second;
    }

    Texture& TextureRepository::getOrCreate(const FrameResource& id, size_t swapchainIndex, vk::ImageUsageFlags textureUsages) {
        if(textures.empty()) {
            textures.resize(driver.getSwapchainImageCount());
        }
        auto it = textures[swapchainIndex].find(id.rootID);
        if(it != textures[swapchainIndex].end()) {
            return *it->second;
        }
        return create(id, swapchainIndex, textureUsages);
    }

    Texture& TextureRepository::create(const FrameResource& resource, size_t frameIndex, vk::ImageUsageFlags textureUsages) {
        // TODO: aliasing
        if(textures.empty()) {
            textures.resize(driver.getSwapchainImageCount());
        }

        auto it = textures[frameIndex].find(resource.rootID);
        if(it == textures[frameIndex].end()) {
            vk::Extent3D size;
            auto& swapchainExtent = driver.getFinalRenderSize();
            switch(resource.size.type) {
                case TextureSize::Type::SwapchainProportional: {
                    size.width = static_cast<std::uint32_t>(resource.size.width * swapchainExtent.width);
                    size.height = static_cast<std::uint32_t>(resource.size.height * swapchainExtent.height);
                    size.depth = static_cast<std::uint32_t>(resource.size.depth * 1);
                } break;

                case TextureSize::Type::Fixed: {
                    size.width = static_cast<std::uint32_t>(resource.size.width);
                    size.height = static_cast<std::uint32_t>(resource.size.height);
                    size.depth = static_cast<std::uint32_t>(resource.size.depth);
                } break;
            }
            auto format = resource.format;

            Texture::Ref texture;

            switch(resource.imageOrigin) {
                case Render::ImageOrigin::SurfaceSwapchain:
                    texture = driver.getSwapchainTextures()[frameIndex];
                    break;

                case Render::ImageOrigin::Created:
                    texture = std::make_shared<Texture>(driver,
                                                        size,
                                                        textureUsages,
                                                        format
                    );
                    break;

                default:
                    TODO
                    break;
            }
            textures[frameIndex][resource.rootID] = std::move(texture);
        } else {
            throw std::runtime_error("Texture already exists");
        }

        return get(resource, frameIndex);
    }

    void TextureRepository::onSwapchainImageCountChange(size_t newCount) {
        textures.clear();
    }

    void TextureRepository::onSwapchainSizeChange(int newWidth, int newHeight) {
        textures.clear();
    }

    vk::ImageUsageFlags& TextureRepository::getUsages(const UUID& id) {
        return usages[id];
    }

#ifdef ENABLE_VR
    void TextureRepository::setXRSession(VR::Session *session) {
        assert(session != nullptr);
        vrSession = session;
    }
#endif
}