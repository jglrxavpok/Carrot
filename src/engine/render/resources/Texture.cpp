//
// Created by jglrxavpok on 18/06/2021.
//

#include "Texture.h"
#include "engine/utils/Assert.h"
#include "engine/Engine.h"

namespace Carrot::Render {
    Texture::Texture(Carrot::VulkanDriver& driver): driver(driver) {}

    Texture::Texture(Carrot::VulkanDriver& driver,
            vk::Extent3D extent,
            vk::ImageUsageFlags usage,
            vk::Format format,
            const set<uint32_t>& families,
            vk::ImageCreateFlags flags,
            vk::ImageType type,
            uint32_t layerCount): driver(driver), imageFormat(format), image(std::make_unique<Carrot::Image>(driver, extent, usage, format, families, flags, type, layerCount)) {
        currentLayout = vk::ImageLayout::eUndefined;
    }

    Texture::Texture(Texture&& toMove): driver(toMove.driver) {
        *this = std::move(toMove);
    }

    Texture::Texture(Carrot::VulkanDriver& driver, const Resource& resource, FileFormat format): driver(driver) {
        runtimeAssert(Carrot::IO::isImageFormat(format), "Format must be an image format!");
        image = Carrot::Image::fromFile(driver, resource);
        image->name(resource.getName());
        currentLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        imageFormat = image->getFormat();
    }

    Texture::Texture(VulkanDriver& driver, vk::Image image, vk::Extent3D extent, vk::Format format, uint32_t layerCount)
    : driver(driver), image(std::make_unique<Carrot::Image>(driver, image, extent, format, layerCount)) {
        imageFormat = this->image->getFormat();
    }

    Carrot::Image& Texture::getImage() {
        runtimeAssert(image, "Texture not initialized!");
        return *image;
    }

    const Carrot::Image& Texture::getImage() const {
        runtimeAssert(image, "Texture not initialized!");
        return *image;
    }

    void Texture::transitionNow(vk::ImageLayout newLayout, vk::ImageAspectFlags aspect) {
        driver.performSingleTimeGraphicsCommands([&](vk::CommandBuffer& cmds) {
            transitionInline(cmds, newLayout, aspect);
        });
    }

    void Texture::transitionInline(vk::CommandBuffer& commands, vk::ImageLayout newLayout, vk::ImageAspectFlags aspect) {
        runtimeAssert(image, "Texture not initialized!");
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
        if(imguiID == nullptr) {
            imguiID = ImGui_ImplVulkan_AddTexture(driver.getLinearSampler(), getView(format, aspect),
                                                  static_cast<VkImageLayout>(vk::ImageLayout::eShaderReadOnlyOptimal));
        }
        return imguiID;
    }

    vk::ImageView Texture::getView(vk::ImageAspectFlags aspect) const {
        return getView(imageFormat, aspect);
    }

    vk::ImageView Texture::getView(vk::Format format, vk::ImageAspectFlags aspect) const {
        auto& view = views[{format, aspect}];

        if(!view) {
            view = image->createImageView(format, aspect);
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
}
