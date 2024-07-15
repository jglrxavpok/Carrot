//
// Created by jglrxavpok on 04/11/2022.
//
#include "TextureCompression.h"
#include "core/utils/stringmanip.h"
#include <ktx.h>
#include <vulkan/vulkan_core.h>

#include <stb_image.h>
#include <core/Macros.h>
#include <core/containers/Vector.hpp>
#include <core/render/ImageFormats.h>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace Fertilizer {
    ConversionResult compressTexture(const std::filesystem::path& inputFile, const std::filesystem::path& outputFile) {
        int w, h, srcComponentCount;
        stbi_uc* pixels = stbi_load(inputFile.string().c_str(), &w, &h, &srcComponentCount, 0);
        CLEANUP(stbi_image_free(pixels));
        std::size_t srcSize = w * h * srcComponentCount;

        if(w % 4 != 0 || h % 4 != 0) {
            return {
                .errorCode = ConversionResultError::TextureCompressionError,
                .errorMessage = Carrot::sprintf("Texture resolution of %s is not a multiple of 4x4 (is %dx%d). This is necessary for BC compression", inputFile.u8string().c_str(), w, h),
            };
        }

        ktxTexture2* texture;
        ktxTextureCreateInfo createInfo;
        KTX_error_code result;
        ktx_uint32_t level, layer, faceSlice;

        createInfo.glInternalformat = 0;  //Ignored as we'll create a KTX2 texture.

        int destComponentCount = srcComponentCount;
        switch(srcComponentCount) {
            case 1:
                createInfo.vkFormat = VK_FORMAT_R8_UNORM;
                break;
            case 2:
                createInfo.vkFormat = VK_FORMAT_R8G8_UNORM;
                break;
            case 3:
            case 4:
                destComponentCount = 4;
                createInfo.vkFormat = VK_FORMAT_R8G8B8A8_UNORM; // RGBA is better supported
                break;
            default:
                return {
                    .errorCode = ConversionResultError::UnsupportedInputType,
                    .errorMessage = Carrot::sprintf("Unsupported channel count: %d", srcComponentCount),
                };
        }
        createInfo.baseWidth = w;
        createInfo.baseHeight = h;
        createInfo.baseDepth = 1;
        createInfo.numDimensions = 2;

        const VkFormat format = static_cast<VkFormat>(createInfo.vkFormat);
        std::uint8_t mipCount = Carrot::ImageFormats::computeMipCount(createInfo.baseWidth, createInfo.baseWidth, createInfo.baseDepth, format);
        createInfo.numLevels = mipCount;
        createInfo.numLayers = 1;
        createInfo.numFaces = 1;
        createInfo.isArray = KTX_FALSE;
        createInfo.generateMipmaps = KTX_FALSE;

        result = ktxTexture2_Create(&createInfo,
                                    KTX_TEXTURE_CREATE_ALLOC_STORAGE,
                                    &texture);

        if(result != ktx_error_code_e::KTX_SUCCESS) {
            return {
                    .errorCode = ConversionResultError::TextureCompressionError,
                    .errorMessage = ktxErrorString(result),
            };
        }

        Carrot::Vector<std::uint8_t> mip0Pixels;
        mip0Pixels.resize(w * h * destComponentCount);
        for(std::uint32_t pixelIndex = 0; pixelIndex < w * h; pixelIndex++) {
            mip0Pixels[pixelIndex * destComponentCount + 0] = pixels[pixelIndex * srcComponentCount + 0];
            if(destComponentCount > 1) mip0Pixels[pixelIndex * destComponentCount + 1] = srcComponentCount > 1 ? pixels[pixelIndex * srcComponentCount + 1] : 0;
            if(destComponentCount > 2) mip0Pixels[pixelIndex * destComponentCount + 2] = srcComponentCount > 2 ? pixels[pixelIndex * srcComponentCount + 2] : 0;
            if(destComponentCount > 3) mip0Pixels[pixelIndex * destComponentCount + 3] = srcComponentCount > 3 ? pixels[pixelIndex * srcComponentCount + 3] : 255;
        }

        level = 0;
        layer = 0;
        faceSlice = 0;
        result = ktxTexture_SetImageFromMemory(ktxTexture(texture),
                                               level, layer, faceSlice,
                                               mip0Pixels.data(), mip0Pixels.size());

        if(result != ktx_error_code_e::KTX_SUCCESS) {
            return {
                .errorCode = ConversionResultError::TextureCompressionError,
                .errorMessage = ktxErrorString(result),
            };
        }

        // Assume all GPUs targeted by Carrot support BC compression (they better)
        VkFormat targetFormat = VK_FORMAT_BC3_UNORM_BLOCK; // TODO: support for different formats (normal maps for example)

        // create uncompressed mips
        using MipData = Carrot::Vector<glm::vec4>; // assume rgba for now
        Carrot::Vector<MipData> uncompressedMips;
        uncompressedMips.resize(mipCount);

        // convert int pixels to vec4
        uncompressedMips[0].resize(w*h);
        for(std::size_t index = 0; index < uncompressedMips[0].size(); index++) {
            std::uint8_t red = mip0Pixels[index * destComponentCount + 0];
            std::uint8_t green = destComponentCount > 1 ? mip0Pixels[index * destComponentCount + 1] : 0;
            std::uint8_t blue = destComponentCount > 2 ? mip0Pixels[index * destComponentCount + 2] : 0;
            std::uint8_t alpha = destComponentCount > 3 ? mip0Pixels[index * destComponentCount + 3] : 255;
            uncompressedMips[0][index] = glm::vec4 { red / 255.0f, green / 255.0f, blue / 255.0f, alpha / 255.0f };
        }

        VkExtent3D previousMipDimensions = VkExtent3D { createInfo.baseWidth, createInfo.baseHeight, createInfo.baseDepth };
        for(std::uint8_t mip = 1; mip < mipCount; mip++) {
            Carrot::Vector<glm::vec4>& mipColors = uncompressedMips[mip];
            Carrot::Vector<std::uint8_t> mipPixels;
            const VkExtent3D mipDimensions = Carrot::ImageFormats::computeMipDimensions(mip, createInfo.baseWidth, createInfo.baseHeight, createInfo.baseDepth, format);
            const std::size_t mipByteSize = Carrot::ImageFormats::computeMipSize(mipDimensions.width, mipDimensions.height, mipDimensions.depth, format);
            mipColors.resize(mipByteSize);
            mipPixels.resize(mipDimensions.width * mipDimensions.height * mipDimensions.depth * sizeof(std::uint8_t) * destComponentCount);

            const glm::vec3 sizeFactor {
                static_cast<float>(previousMipDimensions.width) / mipDimensions.width,
                static_cast<float>(previousMipDimensions.height) / mipDimensions.height,
                static_cast<float>(previousMipDimensions.depth) / mipDimensions.depth,
            };

            const std::uint8_t previousMip = mip-1;
            auto averageColor = [&](std::uint32_t x, std::uint32_t y, std::uint32_t z) {
                glm::vec4 color { 0.0f };
                std::uint32_t sampleCount = 0;
                for(std::uint32_t pz = z * sizeFactor.z; pz < z * sizeFactor.z + sizeFactor.z; pz++) {
                    for(std::uint32_t py = y * sizeFactor.y; py < y * sizeFactor.y + sizeFactor.y; py++) {
                        for(std::uint32_t px = x * sizeFactor.x; px < x * sizeFactor.x + sizeFactor.x; px++) {
                            if(px >= previousMipDimensions.width
                                || py >= previousMipDimensions.height
                                || pz >= previousMipDimensions.depth
                                ) {
                                continue;
                            }
                            color += uncompressedMips[previousMip][pz * previousMipDimensions.height * previousMipDimensions.width + py * previousMipDimensions.width + px];
                            sampleCount++;
                        }
                    }
                }
                if(sampleCount > 0) {
                    return color / static_cast<float>(sampleCount);
                }
                return color;
            };

            for(std::uint32_t z = 0; z < mipDimensions.depth; z++) {
                for(std::uint32_t y = 0; y < mipDimensions.height; y++) {
                    for(std::uint32_t x = 0; x < mipDimensions.width; x++) {
                        // per pixel average of pixels in mip above
                        glm::vec4 color = averageColor(x, y, z);
                        const std::uint32_t index = z * mipDimensions.height * mipDimensions.width + y * mipDimensions.width + x;
                        mipColors[index] = color;
                        mipPixels[index * destComponentCount + 0] = color.r * 255;
                        if(destComponentCount > 1) mipPixels[index * destComponentCount + 1] = color.g * 255;
                        if(destComponentCount > 2) mipPixels[index * destComponentCount + 2] = color.b * 255;
                        if(destComponentCount > 3) mipPixels[index * destComponentCount + 3] = color.a * 255;
                    }
                }
            }

            // create compressed mip
            //TODO; // TODO: use target format

            result = ktxTexture_SetImageFromMemory(ktxTexture(texture),
                                               mip, layer, faceSlice,
                                               mipPixels.data(), mipPixels.size());
            if(result != ktx_error_code_e::KTX_SUCCESS) {
                return {
                    .errorCode = ConversionResultError::TextureCompressionError,
                    .errorMessage = ktxErrorString(result),
                };
            }

            previousMipDimensions = mipDimensions;
        }

        ktxTexture_WriteToNamedFile(ktxTexture(texture), outputFile.string().c_str());
        ktxTexture_Destroy(ktxTexture(texture));

        return result == ktx_error_code_e::KTX_SUCCESS
        ? ConversionResult {
            .errorCode = ConversionResultError::Success,
        }
        : ConversionResult {
            .errorCode = ConversionResultError::TextureCompressionError,
            .errorMessage = ktxErrorString(result),
        };
    }
}
