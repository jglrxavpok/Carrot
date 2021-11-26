//
// Created by jglrxavpok on 10/07/2021.
//

#pragma once
#include <vector>
#include <unordered_map>
#include "engine/render/resources/Texture.h"
#include "engine/render/RenderPassData.h"
#include "engine/utils/UUID.h"

#ifdef ENABLE_VR
namespace Carrot::VR {
    class Session;
}
#endif

namespace Carrot::Render {
    class TextureRepository: public SwapchainAware {
    public:
        TextureRepository(VulkanDriver& driver): driver(driver) {}

    public:
        Texture& create(const FrameResource& texture, size_t swapchainIndex, vk::ImageUsageFlags textureUsages);
        Texture& get(const FrameResource& texture, size_t swapchainIndex);
        Texture& get(const Carrot::UUID& id, size_t swapchainIndex);
        Texture& getOrCreate(const FrameResource& id, size_t swapchainIndex, vk::ImageUsageFlags textureUsages);
        vk::ImageUsageFlags& getUsages(const Carrot::UUID& id);

    public:
        void onSwapchainImageCountChange(size_t newCount) override;

        void onSwapchainSizeChange(int newWidth, int newHeight) override;

#ifdef ENABLE_VR
    public:
        void setXRSession(VR::Session* session);
#endif

    private:
        VulkanDriver& driver;
        std::vector<std::unordered_map<Carrot::UUID, Carrot::Render::Texture::Ref>> textures;
        std::unordered_map<Carrot::UUID, vk::ImageUsageFlags> usages;

#ifdef ENABLE_VR
        VR::Session* vrSession = nullptr;
#endif
    };
}


