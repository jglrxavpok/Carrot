//
// Created by jglrxavpok on 07/07/2024.
//

#include "ImageFormats.h"

#include <array>
#include <core/Macros.h>

namespace Carrot::ImageFormats {
    enum class FormatClass {
        eNone = 0,
        e8bit,
        e16bit,
        e8bitAlpha,
        e24bit,
        e32bit,
        e48bit,
        e64bit,
        e96bit,
        e128bit,
        e192bit,
        e256bit,

        eD16,
        eD24,
        eD32,
        eS8,
        eD16S8,
        eD24S8,
        eD32S8,

        eBC1RGB,
        eBC1RGBA,
        eBC2,
        eBC3,
        eBC4,
        eBC5,
        eBC6H,
        eBC7,
    };

    struct ImageFormat {
        /// "class" of format. All formats with the same class are compatible
        FormatClass formatClass = FormatClass::eNone;

        /// Size of block in bytes
        std::size_t blockSize = static_cast<std::size_t>(-1);

        /// Size of block in texels
        VkExtent3D blockExtent;
    };

    /*key is VkFormat*/
    constexpr auto LookupTableGenerator() {
        // From: https://registry.khronos.org/vulkan/specs/1.3-extensions/html/vkspec.html#formats-compatibility
        std::array<ImageFormat, 256> formats{};

        // 8bit
        formats[VK_FORMAT_R4G4_UNORM_PACK8] = { FormatClass::e8bit, 1, { 1, 1, 1} };
        formats[VK_FORMAT_R8_UNORM] = { FormatClass::e8bit, 1, { 1, 1, 1} };
        formats[VK_FORMAT_R8_SNORM] = { FormatClass::e8bit, 1, { 1, 1, 1} };
        formats[VK_FORMAT_R8_USCALED] = { FormatClass::e8bit, 1, { 1, 1, 1} };
        formats[VK_FORMAT_R8_SSCALED] = { FormatClass::e8bit, 1, { 1, 1, 1} };
        formats[VK_FORMAT_R8_UINT] = { FormatClass::e8bit, 1, { 1, 1, 1} };
        formats[VK_FORMAT_R8_SINT] = { FormatClass::e8bit, 1, { 1, 1, 1} };
        formats[VK_FORMAT_R8_SRGB] = { FormatClass::e8bit, 1, { 1, 1, 1} };

        // 16bit
        // outside of 256 range: formats[VK_FORMAT_A1B5G5R5_UNORM_PACK16_KHR] = { FormatClass::e16bit, 2, { 1, 1, 1 } };
        // outside of 256 range: formats[VK_FORMAT_R10X6_UNORM_PACK16] = { FormatClass::e16bit, 2, { 1, 1, 1 } };
        // outside of 256 range: formats[VK_FORMAT_R12X4_UNORM_PACK16] = { FormatClass::e16bit, 2, { 1, 1, 1 } };
        // outside of 256 range: formats[VK_FORMAT_A4R4G4B4_UNORM_PACK16] = { FormatClass::e16bit, 2, { 1, 1, 1 } };
        // outside of 256 range: formats[VK_FORMAT_A4B4G4R4_UNORM_PACK16] = { FormatClass::e16bit, 2, { 1, 1, 1 } };
        formats[VK_FORMAT_R4G4B4A4_UNORM_PACK16] = { FormatClass::e16bit, 2, { 1, 1, 1 } };
        formats[VK_FORMAT_B4G4R4A4_UNORM_PACK16] = { FormatClass::e16bit, 2, { 1, 1, 1 } };
        formats[VK_FORMAT_R5G6B5_UNORM_PACK16] = { FormatClass::e16bit, 2, { 1, 1, 1 } };
        formats[VK_FORMAT_B5G6R5_UNORM_PACK16] = { FormatClass::e16bit, 2, { 1, 1, 1 } };
        formats[VK_FORMAT_R5G5B5A1_UNORM_PACK16] = { FormatClass::e16bit, 2, { 1, 1, 1 } };
        formats[VK_FORMAT_B5G5R5A1_UNORM_PACK16] = { FormatClass::e16bit, 2, { 1, 1, 1 } };
        formats[VK_FORMAT_A1R5G5B5_UNORM_PACK16] = { FormatClass::e16bit, 2, { 1, 1, 1 } };
        formats[VK_FORMAT_R8G8_UNORM] = { FormatClass::e16bit, 2, { 1, 1, 1 } };
        formats[VK_FORMAT_R8G8_SNORM] = { FormatClass::e16bit, 2, { 1, 1, 1 } };
        formats[VK_FORMAT_R8G8_USCALED] = { FormatClass::e16bit, 2, { 1, 1, 1 } };
        formats[VK_FORMAT_R8G8_SSCALED] = { FormatClass::e16bit, 2, { 1, 1, 1 } };
        formats[VK_FORMAT_R8G8_UINT] = { FormatClass::e16bit, 2, { 1, 1, 1 } };
        formats[VK_FORMAT_R8G8_SINT] = { FormatClass::e16bit, 2, { 1, 1, 1 } };
        formats[VK_FORMAT_R8G8_SRGB] = { FormatClass::e16bit, 2, { 1, 1, 1 } };
        formats[VK_FORMAT_R16_UNORM] = { FormatClass::e16bit, 2, { 1, 1, 1 } };
        formats[VK_FORMAT_R16_SNORM] = { FormatClass::e16bit, 2, { 1, 1, 1 } };
        formats[VK_FORMAT_R16_USCALED] = { FormatClass::e16bit, 2, { 1, 1, 1 } };
        formats[VK_FORMAT_R16_SSCALED] = { FormatClass::e16bit, 2, { 1, 1, 1 } };
        formats[VK_FORMAT_R16_UINT] = { FormatClass::e16bit, 2, { 1, 1, 1 } };
        formats[VK_FORMAT_R16_SINT] = { FormatClass::e16bit, 2, { 1, 1, 1 } };
        formats[VK_FORMAT_R16_SFLOAT] = { FormatClass::e16bit, 2, { 1, 1, 1 } };

        // 8bit alpha
        // outside of 256 range: formats[VK_FORMAT_A8_UNORM_KHR] = { FormatClass::e8bitAlpha, 1, { 1, 1, 1 }};

        // 24bit
        formats[VK_FORMAT_R8G8B8_UNORM] = { FormatClass::e24bit, 3, { 1, 1, 1} };
        formats[VK_FORMAT_R8G8B8_SNORM] = { FormatClass::e24bit, 3, { 1, 1, 1} };
        formats[VK_FORMAT_R8G8B8_USCALED] = { FormatClass::e24bit, 3, { 1, 1, 1} };
        formats[VK_FORMAT_R8G8B8_SSCALED] = { FormatClass::e24bit, 3, { 1, 1, 1} };
        formats[VK_FORMAT_R8G8B8_UINT] = { FormatClass::e24bit, 3, { 1, 1, 1} };
        formats[VK_FORMAT_R8G8B8_SINT] = { FormatClass::e24bit, 3, { 1, 1, 1} };
        formats[VK_FORMAT_R8G8B8_SRGB] = { FormatClass::e24bit, 3, { 1, 1, 1} };
        formats[VK_FORMAT_B8G8R8_UNORM] = { FormatClass::e24bit, 3, { 1, 1, 1} };
        formats[VK_FORMAT_B8G8R8_SNORM] = { FormatClass::e24bit, 3, { 1, 1, 1} };
        formats[VK_FORMAT_B8G8R8_USCALED] = { FormatClass::e24bit, 3, { 1, 1, 1} };
        formats[VK_FORMAT_B8G8R8_SSCALED] = { FormatClass::e24bit, 3, { 1, 1, 1} };
        formats[VK_FORMAT_B8G8R8_UINT] = { FormatClass::e24bit, 3, { 1, 1, 1} };
        formats[VK_FORMAT_B8G8R8_SINT] = { FormatClass::e24bit, 3, { 1, 1, 1} };
        formats[VK_FORMAT_B8G8R8_SRGB] = { FormatClass::e24bit, 3, { 1, 1, 1} };

        // 32bit
        // outside of 256 range: formats[VK_FORMAT_R10X6G10X6_UNORM_2PACK16] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        // outside of 256 range: formats[VK_FORMAT_R12X4G12X4_UNORM_2PACK16] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        // apparently not accessible with my current Vulkan SDK: formats[VK_FORMAT_R16G16_SFIXED5_NV] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_R8G8B8A8_UNORM] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_R8G8B8A8_SNORM] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_R8G8B8A8_USCALED] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_R8G8B8A8_SSCALED] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_R8G8B8A8_UINT] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_R8G8B8A8_SINT] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_R8G8B8A8_SRGB] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_B8G8R8A8_UNORM] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_B8G8R8A8_SNORM] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_B8G8R8A8_USCALED] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_B8G8R8A8_SSCALED] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_B8G8R8A8_UINT] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_B8G8R8A8_SINT] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_B8G8R8A8_SRGB] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_A8B8G8R8_UNORM_PACK32] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_A8B8G8R8_SNORM_PACK32] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_A8B8G8R8_USCALED_PACK32] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_A8B8G8R8_SSCALED_PACK32] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_A8B8G8R8_UINT_PACK32] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_A8B8G8R8_SINT_PACK32] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_A8B8G8R8_SRGB_PACK32] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_A2R10G10B10_UNORM_PACK32] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_A2R10G10B10_SNORM_PACK32] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_A2R10G10B10_USCALED_PACK32] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_A2R10G10B10_SSCALED_PACK32] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_A2R10G10B10_UINT_PACK32] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_A2R10G10B10_SINT_PACK32] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_A2B10G10R10_UNORM_PACK32] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_A2B10G10R10_SNORM_PACK32] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_A2B10G10R10_USCALED_PACK32] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_A2B10G10R10_SSCALED_PACK32] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_A2B10G10R10_UINT_PACK32] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_A2B10G10R10_SINT_PACK32] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_R16G16_UNORM] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_R16G16_SNORM] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_R16G16_USCALED] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_R16G16_SSCALED] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_R16G16_UINT] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_R16G16_SINT] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_R16G16_SFLOAT] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_R32_UINT] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_R32_SINT] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_R32_SFLOAT] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_B10G11R11_UFLOAT_PACK32] = { FormatClass::e32bit, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_E5B9G9R9_UFLOAT_PACK32] = { FormatClass::e32bit, 4, { 1, 1, 1 } };

        // 48bit
        formats[VK_FORMAT_R16G16B16_UNORM] = { FormatClass::e48bit, 6, { 1, 1, 1 } };
        formats[VK_FORMAT_R16G16B16_SNORM] = { FormatClass::e48bit, 6, { 1, 1, 1 } };
        formats[VK_FORMAT_R16G16B16_USCALED] = { FormatClass::e48bit, 6, { 1, 1, 1 } };
        formats[VK_FORMAT_R16G16B16_SSCALED] = { FormatClass::e48bit, 6, { 1, 1, 1 } };
        formats[VK_FORMAT_R16G16B16_UINT] = { FormatClass::e48bit, 6, { 1, 1, 1 } };
        formats[VK_FORMAT_R16G16B16_SINT] = { FormatClass::e48bit, 6, { 1, 1, 1 } };
        formats[VK_FORMAT_R16G16B16_SFLOAT] = { FormatClass::e48bit, 6, { 1, 1, 1 } };

        // 64bit
        formats[VK_FORMAT_R16G16B16A16_UNORM] = { FormatClass::e64bit, 8, { 1, 1, 1 } };
        formats[VK_FORMAT_R16G16B16A16_SNORM] = { FormatClass::e64bit, 8, { 1, 1, 1 } };
        formats[VK_FORMAT_R16G16B16A16_USCALED] = { FormatClass::e64bit, 8, { 1, 1, 1 } };
        formats[VK_FORMAT_R16G16B16A16_SSCALED] = { FormatClass::e64bit, 8, { 1, 1, 1 } };
        formats[VK_FORMAT_R16G16B16A16_UINT] = { FormatClass::e64bit, 8, { 1, 1, 1 } };
        formats[VK_FORMAT_R16G16B16A16_SINT] = { FormatClass::e64bit, 8, { 1, 1, 1 } };
        formats[VK_FORMAT_R16G16B16A16_SFLOAT] = { FormatClass::e64bit, 8, { 1, 1, 1 } };
        formats[VK_FORMAT_R32G32_UINT] = { FormatClass::e64bit, 8, { 1, 1, 1 } };
        formats[VK_FORMAT_R32G32_SINT] = { FormatClass::e64bit, 8, { 1, 1, 1 } };
        formats[VK_FORMAT_R32G32_SFLOAT] = { FormatClass::e64bit, 8, { 1, 1, 1 } };
        formats[VK_FORMAT_R64_UINT] = { FormatClass::e64bit, 8, { 1, 1, 1 } };
        formats[VK_FORMAT_R64_SINT] = { FormatClass::e64bit, 8, { 1, 1, 1 } };
        formats[VK_FORMAT_R64_SFLOAT] = { FormatClass::e64bit, 8, { 1, 1, 1 } };

        // 96bit
        formats[VK_FORMAT_R32G32B32_UINT] = { FormatClass::e96bit, 12, { 1, 1, 1 } };
        formats[VK_FORMAT_R32G32B32_SINT] = { FormatClass::e96bit, 12, { 1, 1, 1 } };
        formats[VK_FORMAT_R32G32B32_SFLOAT] = { FormatClass::e96bit, 12, { 1, 1, 1 } };

        // 128bit
        formats[VK_FORMAT_R32G32B32A32_UINT] = { FormatClass::e128bit, 16, { 1, 1, 1 } };
        formats[VK_FORMAT_R32G32B32A32_SINT] = { FormatClass::e128bit, 16, { 1, 1, 1 } };
        formats[VK_FORMAT_R32G32B32A32_SFLOAT] = { FormatClass::e128bit, 16, { 1, 1, 1 } };
        formats[VK_FORMAT_R64G64_UINT] = { FormatClass::e128bit, 16, { 1, 1, 1 } };
        formats[VK_FORMAT_R64G64_SINT] = { FormatClass::e128bit, 16, { 1, 1, 1 } };
        formats[VK_FORMAT_R64G64_SFLOAT] = { FormatClass::e128bit, 16, { 1, 1, 1 } };

        // 192bit
        formats[VK_FORMAT_R64G64B64_UINT] = { FormatClass::e192bit, 24, { 1, 1, 1 } };
        formats[VK_FORMAT_R64G64B64_SINT] = { FormatClass::e192bit, 24, { 1, 1, 1 } };
        formats[VK_FORMAT_R64G64B64_SFLOAT] = { FormatClass::e192bit, 24, { 1, 1, 1 } };

        // 256bit
        formats[VK_FORMAT_R64G64B64A64_UINT] = { FormatClass::e256bit, 32, { 1, 1, 1 } };
        formats[VK_FORMAT_R64G64B64A64_SINT] = { FormatClass::e256bit, 32, { 1, 1, 1 } };
        formats[VK_FORMAT_R64G64B64A64_SFLOAT] = { FormatClass::e256bit, 32, { 1, 1, 1 } };

        // Depth formats
        formats[VK_FORMAT_D16_UNORM] = { FormatClass::eD16, 2, { 1, 1, 1 } };
        formats[VK_FORMAT_X8_D24_UNORM_PACK32] = { FormatClass::eD24, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_D32_SFLOAT] = { FormatClass::eD32, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_S8_UINT] = { FormatClass::eS8, 1, { 1, 1, 1 } };
        formats[VK_FORMAT_D16_UNORM_S8_UINT] = { FormatClass::eD16S8, 3, { 1, 1, 1 } };
        formats[VK_FORMAT_D24_UNORM_S8_UINT] = { FormatClass::eD24S8, 4, { 1, 1, 1 } };
        formats[VK_FORMAT_D32_SFLOAT_S8_UINT] = { FormatClass::eD32S8, 5, { 1, 1, 1 } };

        // BC1_RGB
        formats[VK_FORMAT_BC1_RGB_UNORM_BLOCK] = { FormatClass::eBC1RGB, 8, { 4, 4, 1 } };
        formats[VK_FORMAT_BC1_RGB_SRGB_BLOCK] = { FormatClass::eBC1RGB, 8, { 4, 4, 1 } };

        // BC1_RGBA
        formats[VK_FORMAT_BC1_RGBA_UNORM_BLOCK] = { FormatClass::eBC1RGBA, 8, { 4, 4, 1 } };
        formats[VK_FORMAT_BC1_RGBA_SRGB_BLOCK] = { FormatClass::eBC1RGBA, 8, { 4, 4, 1 } };

        // BC2
        formats[VK_FORMAT_BC2_UNORM_BLOCK] = { FormatClass::eBC2, 16, { 4, 4, 1 } };
        formats[VK_FORMAT_BC2_SRGB_BLOCK] = { FormatClass::eBC2, 16, { 4, 4, 1 } };

        // BC3
        formats[VK_FORMAT_BC3_UNORM_BLOCK] = { FormatClass::eBC3, 16, { 4, 4, 1 } };
        formats[VK_FORMAT_BC3_SRGB_BLOCK] = { FormatClass::eBC3, 16, { 4, 4, 1 } };

        // BC4
        formats[VK_FORMAT_BC4_UNORM_BLOCK] = { FormatClass::eBC4, 8, { 4, 4, 1 } };
        formats[VK_FORMAT_BC4_SNORM_BLOCK] = { FormatClass::eBC4, 8, { 4, 4, 1 } };

        // BC5
        formats[VK_FORMAT_BC5_UNORM_BLOCK] = { FormatClass::eBC5, 16, { 4, 4, 1 } };
        formats[VK_FORMAT_BC5_SNORM_BLOCK] = { FormatClass::eBC5, 16, { 4, 4, 1 } };

        // BC6H
        formats[VK_FORMAT_BC6H_UFLOAT_BLOCK] = { FormatClass::eBC6H, 16, { 4, 4, 1 } };
        formats[VK_FORMAT_BC6H_SFLOAT_BLOCK] = { FormatClass::eBC6H, 16, { 4, 4, 1 } };

        // BC7
        formats[VK_FORMAT_BC7_UNORM_BLOCK] = { FormatClass::eBC7, 16, { 4, 4, 1 } };
        formats[VK_FORMAT_BC7_SRGB_BLOCK] = { FormatClass::eBC7, 16, { 4, 4, 1 } };

        // Carrot stops at BC7

        return formats;
    };

    static constexpr std::array<ImageFormat, 256> Formats = LookupTableGenerator();

    static const ImageFormat& getFormat(VkFormat format) {
        VkFormat vkFormat = static_cast<VkFormat>(format);
        verify(vkFormat < 256, "Format not known by Carrot!");
        const auto& formatInfo = Formats[vkFormat];
        verify(formatInfo.formatClass != FormatClass::eNone, "Format not known by Carrot!");

        return formatInfo;
    }

    std::size_t computeMipSize(std::uint32_t mipWidth, std::uint32_t mipHeight, std::uint32_t mipDepth, VkFormat imageFormat) {
        const ImageFormat& format = getFormat(imageFormat);
        std::size_t blockWidth = ceil(mipWidth / static_cast<float>(format.blockExtent.width));
        std::size_t blockHeight = ceil(mipHeight / static_cast<float>(format.blockExtent.height));
        std::size_t blockDepth = ceil(mipDepth / static_cast<float>(format.blockExtent.depth));

        return blockWidth * blockHeight * blockDepth * format.blockSize;
    }

    std::size_t computeMipSize(std::uint32_t mipLevel, std::uint32_t imageWidth, std::uint32_t imageHeight, std::uint32_t imageDepth, VkFormat imageFormat) {
        const ImageFormat& format = getFormat(imageFormat);
        VkExtent3D minSize = format.blockExtent;
        while(mipLevel > 0) {
            imageWidth = std::max(minSize.width, imageWidth >> 1);
            imageHeight = std::max(minSize.height, imageHeight >> 1);
            imageDepth = std::max(minSize.depth, imageDepth >> 1);
            mipLevel--;
        }

        return computeMipSize(imageWidth, imageHeight, imageDepth, imageFormat);
    }

    std::uint8_t computeMipCount(std::uint32_t imageWidth, std::uint32_t imageHeight, std::uint32_t imageDepth, VkFormat imageFormat) {
        const ImageFormat& format = getFormat(imageFormat);
        VkExtent3D minSize = format.blockExtent;
        std::uint8_t mipLevel = 1;
        while(imageWidth > minSize.width || imageHeight > minSize.height || imageDepth > minSize.depth) {
            imageWidth = std::max(minSize.width, imageWidth >> 1);
            imageHeight = std::max(minSize.height, imageHeight >> 1);
            imageDepth = std::max(minSize.depth, imageDepth >> 1);
            mipLevel++;
        }
        return mipLevel;
    }

    VkExtent3D computeMipDimensions(std::uint32_t mipLevel, std::uint32_t imageWidth, std::uint32_t imageHeight, std::uint32_t imageDepth, VkFormat imageFormat) {
        const ImageFormat& format = getFormat(imageFormat);
        VkExtent3D minSize = format.blockExtent;
        while(mipLevel > 0) {
            imageWidth = std::max(minSize.width, imageWidth >> 1);
            imageHeight = std::max(minSize.height, imageHeight >> 1);
            imageDepth = std::max(minSize.depth, imageDepth >> 1);
            mipLevel--;
        }
        return VkExtent3D { imageWidth, imageHeight, imageDepth };
    }
} // Carrot