//
// Created by jglrxavpok on 28/11/2020.
//

#pragma once

#include "engine/vulkan/includes.h"
#include "engine/vulkan/DebugNameable.h"
#include "engine/vulkan/VulkanDriver.h"
#include <memory>
#include <set>
#include <functional>
#include <engine/render/Skybox.hpp>
#include "engine/io/Resource.h"

namespace Carrot {
    /// Abstraction over Vulkan images. Manages lifetime and memory
    class Image: public DebugNameable {
    private:
        Carrot::VulkanDriver& driver;
        vk::Extent3D size{};

        struct ImageData {
            bool ownsImage = false;

            //union {
                struct {
                    vk::UniqueImage vkImage = {};
                    vk::UniqueDeviceMemory memory = {};
                } asOwned;

                struct {
                    vk::Image vkImage = nullptr;
                } asView;
            //};

            explicit ImageData(bool owns): ownsImage(owns) {
                if(owns) {
                    asOwned = {};
                } else {
                    asView = {};
                }
            }

            ~ImageData() {
                if(ownsImage) {
                    asOwned.memory.reset();
                    asOwned.vkImage.reset();
                }
            }

        } imageData;

        uint32_t layerCount = 1;
        vk::Format format = vk::Format::eUndefined;

    public:
        /// Creates a new empty image with the given parameters. Will also allocate the corresponding memory
        explicit Image(Carrot::VulkanDriver& driver,
                       vk::Extent3D extent,
                       vk::ImageUsageFlags usage,
                       vk::Format format,
                       set<uint32_t> families = {},
                       vk::ImageCreateFlags flags = static_cast<vk::ImageCreateFlags>(0),
                       vk::ImageType type = vk::ImageType::e2D,
                       uint32_t layerCount = 1);

        explicit Image(Carrot::VulkanDriver& driver, vk::Image toView,
                       vk::Extent3D extent,
                       vk::Format format,
                       std::uint32_t layerCount = 1);

        const vk::Image& getVulkanImage() const;
        const vk::DeviceMemory& getMemory() const;
        const vk::Extent3D& getSize() const;
        vk::Format getFormat() const;

        /// Stage a upload to this image, and wait for the upload to finish.
        void stageUpload(const vector<uint8_t>& data, uint32_t layer = 0, uint32_t layerCount = 1);

        /// Transition the layout of this image from one layout to another
        void transitionLayout(vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout);

        /// Transition the layout of this image from one layout to another, inside of a given command buffer
        void transitionLayoutInline(vk::CommandBuffer& commands, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor);

        static void transition(vk::Image image, vk::CommandBuffer& commands, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, uint32_t layerCount = 1, vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor);

        /// Creates a ImageView pointing to this image
        vk::UniqueImageView createImageView(vk::Format imageFormat = vk::Format::eR8G8B8A8Unorm, vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor);

        /// Create and fill an Image from a given image file
        static unique_ptr<Image> fromFile(Carrot::VulkanDriver& device, const Carrot::IO::Resource resource);

        static unique_ptr<Image> cubemapFromFiles(Carrot::VulkanDriver& device, std::function<std::string(Skybox::Direction)> textureSupplier);

        ~Image() = default;

    protected:
        void setDebugNames(const string& name) override;
    };

}
