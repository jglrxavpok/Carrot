//
// Created by jglrxavpok on 28/11/2020.
//

#pragma once

#include "vulkan/includes.h"
#include "Engine.h"
#include <set>

namespace Carrot {
    class Image {
    private:
        Carrot::Engine& engine;
        vk::Extent3D size{};
        vk::UniqueImage vkImage;
        vk::UniqueDeviceMemory memory;

    public:
        explicit Image(Carrot::Engine& engine,
                       vk::Extent3D extent,
                       vk::ImageUsageFlags usage,
                       vk::Format format,
                       set<uint32_t> families = {},
                       vk::ImageCreateFlags flags = static_cast<vk::ImageCreateFlags>(0),
                       vk::ImageType type = vk::ImageType::e2D);

        const vk::Image& getVulkanImage() const;
        const vk::Extent3D& getSize() const;

        void stageUpload(const vector<uint8_t>& data);

        void transitionLayout(vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout);

        vk::UniqueImageView createImageView();

        static unique_ptr<Image> fromFile(Carrot::Engine& engine, const std::string& filename);

        ~Image() = default;
    };

}
