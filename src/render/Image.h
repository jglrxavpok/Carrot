//
// Created by jglrxavpok on 28/11/2020.
//

#pragma once

#include "vulkan/includes.h"
#include "vulkan/DebugNameable.h"
#include "Engine.h"
#include <set>

namespace Carrot {
    /// Abstraction over Vulkan images. Manages lifetime and memory
    class Image: public DebugNameable {
    private:
        Carrot::Engine& engine;
        vk::Extent3D size{};
        vk::UniqueImage vkImage;
        vk::UniqueDeviceMemory memory;

    public:
        /// Creates a new empty image with the given parameters. Will also allocate the corresponding memory
        explicit Image(Carrot::Engine& engine,
                       vk::Extent3D extent,
                       vk::ImageUsageFlags usage,
                       vk::Format format,
                       set<uint32_t> families = {},
                       vk::ImageCreateFlags flags = static_cast<vk::ImageCreateFlags>(0),
                       vk::ImageType type = vk::ImageType::e2D);

        const vk::Image& getVulkanImage() const;
        const vk::Extent3D& getSize() const;

        /// Stage a upload to this image, and wait for the upload to finish.
        void stageUpload(const vector<uint8_t>& data);

        /// Transition the layout of this image from one layout to another
        void transitionLayout(vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout);

        /// Creates a ImageView pointing to this image
        vk::UniqueImageView createImageView();

        /// Create and fill an Image from a given image file
        static unique_ptr<Image> fromFile(Carrot::Engine& engine, const std::string& filename);

        ~Image() = default;

    protected:
        void setDebugNames(const string& name) override;
    };

}
