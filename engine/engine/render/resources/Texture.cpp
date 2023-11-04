//
// Created by jglrxavpok on 18/06/2021.
//

#include "Texture.h"
#include "core/utils/Assert.h"
#include "engine/Engine.h"

namespace Carrot::Render {
    Texture::Texture(): Texture(GetVulkanDriver()) {}
    Texture::Texture(Carrot::VulkanDriver& driver): resource(), driver(driver) {}

    Texture::Texture(Carrot::VulkanDriver& driver,
            vk::Extent3D extent,
            vk::ImageUsageFlags usage,
            vk::Format format,
            const std::set<std::uint32_t>& families,
            vk::ImageCreateFlags flags,
            vk::ImageType type,
                     std::uint32_t layerCount): resource(), driver(driver), imageFormat(format), image(std::make_unique<Carrot::Image>(driver, extent, usage, format, families, flags, type, layerCount)) {
        currentLayout = vk::ImageLayout::eUndefined;
    }

    Texture::Texture(Texture&& toMove): driver(toMove.driver), resource(std::move(toMove.resource)) {
        *this = std::move(toMove);
    }

    Texture::Texture(Carrot::VulkanDriver& driver, const Resource& resource, FileFormat format): driver(driver), resource(resource) {
        verify(Carrot::IO::isImageFormat(format), "Format must be an image format!");
        image = Carrot::Image::fromFile(driver, resource);
        image->name(resource.getName());
        currentLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        imageFormat = image->getFormat();
    }

    Texture::Texture(VulkanDriver& driver, vk::Image image, vk::Extent3D extent, vk::Format format, std::uint32_t layerCount)
    : resource(), driver(driver), image(std::make_unique<Carrot::Image>(driver, image, extent, format, layerCount)) {
        imageFormat = this->image->getFormat();
    }

    Texture::Texture(const Carrot::Image& image): Texture(image.getDriver(), image.getVulkanImage(), image.getSize(), image.getFormat(), image.getLayerCount()) {

    }

    Texture::Texture(std::unique_ptr<Carrot::Image>&& image): driver(image->getDriver()), image(std::move(image)) {
        imageFormat = getImage().getFormat();
        currentLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    }

    Texture::~Texture() {
        for(auto& [k, imageView] : views) {
            GetVulkanDriver().deferDestroy("-imageview", std::move(imageView));
        }
    }

    Carrot::Image& Texture::getImage() {
        verify(image, "Texture not initialized!");
        return *image;
    }

    const Carrot::Image& Texture::getImage() const {
        verify(image, "Texture not initialized!");
        return *image;
    }

    void Texture::transitionNow(vk::ImageLayout newLayout, vk::ImageAspectFlags aspect) {
        driver.performSingleTimeGraphicsCommands([&](vk::CommandBuffer& cmds) {
            transitionInline(cmds, newLayout, aspect);
        });
    }

    void Texture::transitionInline(vk::CommandBuffer& commands, vk::ImageLayout newLayout, vk::ImageAspectFlags aspect) {
        verify(image, "Texture not initialized!");
        if(currentLayout == newLayout)
            return;

        image->transitionLayoutInline(commands, currentLayout, newLayout, aspect);
        currentLayout = newLayout;
    }

    void Texture::assumeLayout(vk::ImageLayout newLayout) {
        currentLayout = newLayout;
    }

    const vk::Image& Texture::getVulkanImage() const {
        return getImage().getVulkanImage();
    }

    const vk::Extent3D& Texture::getSize() const {
        return getImage().getSize();
    }

    ImTextureID Texture::getImguiID(vk::ImageAspectFlags aspect) const {
        return getImguiID(imageFormat, aspect);
    }

    ImTextureID Texture::getImguiID(vk::Format format, vk::ImageAspectFlags aspect) const {
        return (ImTextureID) this;
    }

    const vk::ImageView& Texture::getView(vk::ImageAspectFlags aspect) const {
        return getView(imageFormat, aspect);
    }

    const vk::ImageView& Texture::getView(vk::Format format, vk::ImageAspectFlags aspect, vk::ImageViewType viewType) const {
        auto& view = views[{format, aspect}];

        if(!view) {
            view = image->createImageView(format, aspect, viewType, image->getLayerCount());
        }

        return *view;
    }

    Texture& Texture::operator=(Texture&& toMove) noexcept {
        assert(&driver == &toMove.driver);
        views = std::move(toMove.views);
        image = std::move(toMove.image);
        currentLayout = toMove.currentLayout;
        imageFormat = toMove.imageFormat;
        imguiID = toMove.imguiID;
        return *this;
    }

    void Texture::setDebugNames(const std::string& name) {
        image->name(name);
    }

    void Texture::clear(vk::CommandBuffer& cmds, vk::ClearValue clearValue, vk::ImageAspectFlags aspect) {
        vk::ImageSubresourceRange wholeTexture {
                .aspectMask = aspect,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = image->getLayerCount(),
        };
        if(aspect & vk::ImageAspectFlagBits::eDepth) {
            assert((aspect & vk::ImageAspectFlagBits::eColor) == static_cast<vk::ImageAspectFlagBits>(0));
            cmds.clearDepthStencilImage(getVulkanImage(), currentLayout, clearValue.depthStencil, wholeTexture);
        } else {
            cmds.clearColorImage(getVulkanImage(), currentLayout, clearValue.color, wholeTexture);
        }
    }

    glm::vec<4, std::uint32_t> Texture::sampleUVec4(float u, float v) const {
        auto& engine = Carrot::Engine::getInstance();
        using vectype = glm::vec<4, std::uint32_t>;
        std::int32_t x = u * getSize().width;
        std::int32_t y = v * getSize().height;
        Carrot::Buffer tmpBuffer{engine.getVulkanDriver(), sizeof(vectype), vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible};
        engine.performSingleTimeGraphicsCommands([&](vk::CommandBuffer& cmds) {
            // TODO: move color attachment to parameters
            getImage().transition(getImage().getVulkanImage(), cmds, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eTransferSrcOptimal);
            vk::BufferImageCopy region {
                    .bufferOffset = 0,
                    .bufferRowLength = 0,
                    .bufferImageHeight = 0,
                    .imageSubresource = vk::ImageSubresourceLayers {
                            .aspectMask = vk::ImageAspectFlagBits::eColor,
                            .mipLevel = 0,
                            .baseArrayLayer = 0,
                            .layerCount = 1,
                    },
                    .imageOffset = vk::Offset3D {
                        .x = x,
                        .y = y,
                        .z = 0,
                    },
                    .imageExtent = vk::Extent3D {
                            .width = 1,
                            .height = 1,
                            .depth = 1,
                    }
            };
            cmds.copyImageToBuffer(getImage().getVulkanImage(), vk::ImageLayout::eTransferSrcOptimal, tmpBuffer.getVulkanBuffer(), region);

            // TODO: move color attachment to parameters
            // put image back into previous layout
            getImage().transition(getImage().getVulkanImage(), cmds, vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eColorAttachmentOptimal);
        }, true);
        return *tmpBuffer.map<vectype>();
    }
}
