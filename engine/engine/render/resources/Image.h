//
// Created by jglrxavpok on 28/11/2020.
//

#pragma once

#include <engine/vulkan/DebugNameable.h>
#include <engine/vulkan/VulkanDriver.h>
#include <engine/render/resources/DeviceMemory.h>
#include <memory>
#include <set>
#include <functional>
#include <unordered_set>
#include <engine/render/Skybox.hpp>
#include <core/io/Resource.h>
#include <core/async/Locks.h>

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
                    Carrot::DeviceMemory memory = {};
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
                    asOwned.memory = {};
                    asOwned.vkImage.reset();
                }
            }

        } imageData;

        std::uint32_t layerCount = 1;
        std::uint32_t mipCount = 1;
        vk::Format format = vk::Format::eUndefined;
        vk::ImageUsageFlags usage = static_cast<vk::ImageUsageFlags>(0);

    public:
        static Carrot::Async::SpinLock AliveImagesAccess;
        //! How many images are alive, and where?
        static std::unordered_set<const Image*> AliveImages;

        /// Creates a new empty image with the given parameters. Will also allocate the corresponding memory
        explicit Image(Carrot::VulkanDriver& driver,
                       vk::Extent3D extent,
                       vk::ImageUsageFlags usage,
                       vk::Format format,
                       std::set<uint32_t> families = {},
                       vk::ImageCreateFlags flags = static_cast<vk::ImageCreateFlags>(0),
                       vk::ImageType type = vk::ImageType::e2D,
                       std::uint32_t layerCount = 1,
                       std::uint32_t mipCount = 1);

        explicit Image(Carrot::VulkanDriver& driver, vk::Image toView,
                       vk::Extent3D extent,
                       vk::Format format,
                       std::uint32_t layerCount = 1,
                       std::uint32_t mipCount = 1);

        ~Image() noexcept;

        /// Returns true iif this image was created by the engine, and is not a view to an external VkImage (swapchain, external libs)
        bool isOwned() const;

        const vk::Image& getVulkanImage() const;

        /// Only valid for owned images (ie created by the engine, not swapchain / external libs)
        vk::DeviceMemory getVkMemory() const;

        /// Only valid for owned images (ie created by the engine, not swapchain / external libs)
        const Carrot::DeviceMemory& getMemory() const;

        const vk::Extent3D& getSize() const;
        vk::Format getFormat() const;
        std::uint32_t getLayerCount() const { return layerCount; }
        VulkanDriver& getDriver() const { return driver; }

        /// Stage a upload to this image, and wait for the upload to finish.
        void stageUpload(std::span<std::uint8_t> data, std::uint32_t layer = 0, std::uint32_t layerCount = 1);

        /// Copies the contents of the input buffer to this image, and wait for the upload to finish.
        /// Texture data is expected to be by mip, then by layer, then by row, then by column.
        /// Note: mip 'startMip' is expected to be at the start of the buffer data, and mip data is expected to be tightly packed
        void stageUpload(Carrot::BufferView textureData, std::uint32_t layer, std::uint32_t layerCount, std::uint32_t startMip, std::uint32_t mipCount);

        /// Transition the layout of this image from one layout to another
        void transitionLayout(vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout);

        /// Transition the layout of this image from one layout to another, inside of a given command buffer
        void transitionLayoutInline(vk::CommandBuffer& commands, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor);

        static void transition(vk::Image image, vk::CommandBuffer& commands, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, std::uint32_t layerCount = 1, vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor);

        /// Creates a ImageView pointing to this image
        vk::UniqueImageView createImageView(vk::Format imageFormat = vk::Format::eR8G8B8A8Unorm, vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor, vk::ImageViewType viewType = vk::ImageViewType::e2D, std::uint32_t layerCount = 1);

        /// Create and fill an Image from a given image file
        static std::unique_ptr<Image> fromFile(Carrot::VulkanDriver& device, const Carrot::IO::Resource resource);

        static std::unique_ptr<Image> cubemapFromFiles(Carrot::VulkanDriver& device, std::function<std::string(Skybox::Direction)> textureSupplier);

    public: // mipmap support
        /// From this image's description, compute the size in bytes of the given mipLevel
        std::size_t computeMipDataSize(std::uint32_t mipLevel) const;


    protected:
        void setDebugNames(const std::string& name) override;
    };

}
