//
// Created by jglrxavpok on 18/06/2021.
//

#pragma once
#include <memory>
#include <unordered_map>
#include <imgui.h>
#include "Image.h"
#include "core/io/Resource.h"
#include "core/io/FileFormats.h"
#include <glm/glm.hpp>

namespace sol {
    class state;
}

namespace Carrot::Render {
    using namespace Carrot::IO;

    class Texture: public DebugNameable {
    public:
        using Ptr = std::unique_ptr<Texture>;
        using Ref = std::shared_ptr<Texture>;

        // copies are not allowed
        Texture(const Texture&) = delete;

        Texture(Texture&& toMove);
        Texture& operator=(Texture&& toMove) noexcept;

        /// Create empty texture object
        explicit Texture();
        explicit Texture(Carrot::VulkanDriver& driver);

        /// Create empty texture object with allocated vulkan image
        explicit Texture(Carrot::VulkanDriver& driver,
                         vk::Extent3D extent,
                         vk::ImageUsageFlags usage,
                         vk::Format format,
                         const std::set<uint32_t>& families = {},
                         vk::ImageCreateFlags flags = static_cast<vk::ImageCreateFlags>(0),
                         vk::ImageType type = vk::ImageType::e2D,
                         std::uint32_t layerCount = 1);

        /// Create texture from a given file
        explicit Texture(Carrot::VulkanDriver& driver, const Resource& resource, FileFormat format = FileFormat::PNG);

        /// Wrap vk::Image into a Texture
        explicit Texture(Carrot::VulkanDriver& driver,
                         vk::Image image,
                         vk::Extent3D extent,
                         vk::Format format,
                         std::uint32_t layerCount = 1);

        /// Wrap Carrot::Image into a Texture. DOES NOT own the image
        explicit Texture(const Carrot::Image& image);

        /// Create a texture from a Carrot::Image unique_ptr. Owns the image
        explicit Texture(std::unique_ptr<Carrot::Image>&& image);

        ~Texture();

    public:
        Carrot::Image& getImage();
        const Carrot::Image& getImage() const;

        vk::ImageLayout getCurrentImageLayout() const { return currentLayout; }

        const vk::Image& getVulkanImage() const;
        const vk::Extent3D& getSize() const;

    public:
        /// Gets or create the view with the given aspect
        const vk::ImageView& getView(vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor) const;
        const vk::ImageView& getView(vk::Format format, vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor, vk::ImageViewType viewType = vk::ImageViewType::e2D) const;

    public:
        void transitionNow(vk::ImageLayout newLayout, vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor);
        void transitionInline(vk::CommandBuffer& commands, vk::ImageLayout newLayout, vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor);

        /// Changes the internal tracked layout. Can be used if layout changes without a call to "transitionNow" or "transitionInline"
        void assumeLayout(vk::ImageLayout newLayout);

    public:
        void clear(vk::CommandBuffer& cmds, vk::ClearValue clearValue = vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,0.0f}), vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor);

    public:
        ImTextureID getImguiID(vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor) const;
        ImTextureID getImguiID(vk::Format format, vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor) const;

    public:
        const Carrot::IO::Resource& getOriginatingResource() const { return resource; }

    public:
        glm::vec<4, std::uint32_t> sampleUVec4(float u, float v) const;

    protected:
        void setDebugNames(const std::string& name) override;

    private:
        using FormatAspectPair = std::pair<vk::Format, vk::ImageAspectFlags>;
        struct HashFormatAspectPair {
            size_t operator()(const FormatAspectPair& p) const
            {
                auto hash1 = std::hash<vk::Format>{}(p.first);
                auto hash2 = std::hash<unsigned int>{}(p.second.operator unsigned int());
                return hash1 ^ hash2;
            }
        };

        Carrot::VulkanDriver& driver;
        std::unique_ptr<Carrot::Image> image = nullptr;
        mutable std::unordered_map<FormatAspectPair, vk::UniqueImageView, HashFormatAspectPair> views{};
        vk::ImageLayout currentLayout = vk::ImageLayout::eUndefined;
        vk::Format imageFormat = vk::Format::eUndefined;
        Carrot::IO::Resource resource; // resource from which this texture comes. Used for serialisation

        mutable ImTextureID imguiID = 0;
    };
}
