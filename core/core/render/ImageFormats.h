//
// Created by jglrxavpok on 07/07/2024.
//

#pragma once

#include <cstddef>
#include <cstdint>
#include <vulkan/vulkan_core.h>

namespace Carrot::ImageFormats {
    /// Size in bytes of a single mip of given parameters
    /// Use if you already know the dimensions of the mip
    std::size_t computeMipSize(std::uint32_t mipWidth, std::uint32_t mipHeight, std::uint32_t mipDepth, VkFormat imageFormat);

    /// Size in bytes, of the given mip level, from an image with the given parameters
    /// Use if you don't already know the dimensions of the mip
    std::size_t computeMipSize(std::uint32_t mipLevel, std::uint32_t imageWidth, std::uint32_t imageHeight, std::uint32_t imageDepth, VkFormat imageFormat);

	/// Compute how many mips an image of given size and format can have at maximum
	std::uint8_t computeMipCount(std::uint32_t imageWidth, std::uint32_t imageHeight, std::uint32_t imageDepth, VkFormat imageFormat);

	VkExtent3D computeMipDimensions(std::uint32_t mipLevel, std::uint32_t imageWidth, std::uint32_t imageHeight, std::uint32_t imageDepth, VkFormat imageFormat);

	/// Size in bytes of a 'block' (smallest element of the format, could be a single pixel, or a 4x4 block, or whatever the format wants)
	std::size_t getBlockSize(VkFormat imageFormat);
} // Carrot::ImageFormats
