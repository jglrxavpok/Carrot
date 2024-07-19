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

#define RGBCX_IMPLEMENTATION
#include <array>
#include <rgbcx.h>
#include <core/allocators/StackAllocator.h>

namespace Fertilizer {
    static void initEncoderIfNecessary() {
        static std::once_flag once_flag;
        std::call_once(once_flag, []() {
           rgbcx::init();
        });
    }

    ConversionResult compressTexture(const std::filesystem::path& inputFile, const std::filesystem::path& outputFile) {
        int w, h, srcComponentCount;
        stbi_uc* pixels = stbi_load(inputFile.string().c_str(), &w, &h, &srcComponentCount, 0);
        CLEANUP(stbi_image_free(pixels));
        std::size_t srcSize = w * h * srcComponentCount;
        initEncoderIfNecessary();

        bool canCompress = true;
        if(w % 4 != 0 || h % 4 != 0) {
            canCompress = false;
            /*return {
                .errorCode = ConversionResultError::TextureCompressionError,
                .errorMessage = Carrot::sprintf("Texture resolution of %s is not a multiple of 4x4 (is %dx%d). This is necessary for BC compression", inputFile.u8string().c_str(), w, h),
            };*/
        }

        ktxTexture2* texture;
        ktxTextureCreateInfo createInfo {};
        KTX_error_code result;
        ktx_uint32_t layer = 0;
        ktx_uint32_t faceSlice = 0;

        createInfo.glInternalformat = 0;  //Ignored as we'll create a KTX2 texture.

        int destComponentCount = srcComponentCount;
        VkFormat srcFormat;
        switch(srcComponentCount) {
            case 1:
                srcFormat = VK_FORMAT_R8_UNORM;
                canCompress = false; // TODO
                break;
            case 2:
                srcFormat = VK_FORMAT_R8G8_UNORM;
                canCompress = false; // TODO
                break;
            case 3:
            case 4:
                destComponentCount = 4;
                srcFormat = VK_FORMAT_R8G8B8A8_UNORM; // RGBA is better supported
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

        VkFormat targetFormat = srcFormat;
        if(canCompress) {
            // Assume all GPUs targeted by Carrot support BC compression (they better)
            targetFormat = VK_FORMAT_BC3_UNORM_BLOCK;
        }

        std::uint8_t mipCount = Carrot::ImageFormats::computeMipCount(createInfo.baseWidth, createInfo.baseWidth, createInfo.baseDepth, targetFormat);
        createInfo.numLevels = mipCount;
        createInfo.numLayers = 1;
        createInfo.numFaces = 1;
        createInfo.isArray = KTX_FALSE;
        createInfo.generateMipmaps = KTX_FALSE;
        createInfo.vkFormat = targetFormat;

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

        Carrot::StackAllocator tempMipDataAllocator{ Carrot::Allocator::getDefault() };
        auto encodeMip = [&](std::uint8_t mipLevel, VkExtent3D mipDimensions, const Carrot::Vector<std::uint8_t>& uncompressedMipPixels) {
            ktx_error_code_e result = ktx_error_code_e::KTX_SUCCESS;
            if(srcFormat != targetFormat) {
                // create compressed mip
                // for now only support BC3 (16 bytes/block)
                Carrot::Vector<std::uint8_t> compressedMipPixels { tempMipDataAllocator };
                const std::size_t blockSize = 16;
                const std::size_t compressedMipByteSize = Carrot::ImageFormats::computeMipSize(mipDimensions.width, mipDimensions.height, mipDimensions.depth, targetFormat);
                compressedMipPixels.resize(compressedMipByteSize);

                std::uint32_t depthInBlocks = mipDimensions.depth;
                std::uint32_t heightInBlocks = mipDimensions.height / 4;
                std::uint32_t widthInBlocks = mipDimensions.width / 4;

                auto encodeBC3Block = [&](std::uint32_t blockX, std::uint32_t blockY, std::uint32_t blockZ) {
                    void* pDst = &compressedMipPixels[blockSize * (blockZ * heightInBlocks * widthInBlocks + blockY * widthInBlocks + blockX)];
                    std::array<std::uint8_t, 4*4*4> pixels; // 4x4 blocks of RGBA data
                    const std::uint8_t compCount = 4;
                    for(std::uint32_t dy = 0; dy < 4; dy++) {
                        for(std::uint32_t dx = 0; dx < 4; dx++) {
                            std::uint32_t pixelIndex = blockX * 4 + dx + (blockY * 4 + dy) * mipDimensions.width + blockZ * mipDimensions.width * mipDimensions.height;
                            pixels[compCount * (dx + dy*4) + 0] = uncompressedMipPixels[pixelIndex * destComponentCount + 0];
                            pixels[compCount * (dx + dy*4) + 1] = uncompressedMipPixels[pixelIndex * destComponentCount + 1];
                            pixels[compCount * (dx + dy*4) + 2] = uncompressedMipPixels[pixelIndex * destComponentCount + 2];
                            pixels[compCount * (dx + dy*4) + 3] = uncompressedMipPixels[pixelIndex * destComponentCount + 3];
                        }
                    }
                    rgbcx::encode_bc3(rgbcx::MAX_LEVEL, pDst, pixels.data());
                };
                for(std::uint32_t blockZ = 0; blockZ < depthInBlocks; blockZ++) {
                    for(std::uint32_t blockY = 0; blockY < heightInBlocks; blockY++) {
                        for(std::uint32_t blockX = 0; blockX < widthInBlocks; blockX++) {
                            encodeBC3Block(blockX, blockY, blockZ);
                        }
                    }
                }
                result = ktxTexture_SetImageFromMemory(ktxTexture(texture),
                                   mipLevel, layer, faceSlice,
                                   compressedMipPixels.data(), compressedMipPixels.size());
            } else {
                result = ktxTexture_SetImageFromMemory(ktxTexture(texture),
                                                   mipLevel, layer, faceSlice,
                                                   uncompressedMipPixels.cdata(), uncompressedMipPixels.size());
            }

            return result;
        };

        result = encodeMip(0, VkExtent3D { createInfo.baseWidth, createInfo.baseHeight, createInfo.baseDepth }, mip0Pixels);
        if(result != ktx_error_code_e::KTX_SUCCESS) {
            return {
                .errorCode = ConversionResultError::TextureCompressionError,
                .errorMessage = ktxErrorString(result),
            };
        }

        VkExtent3D previousMipDimensions = VkExtent3D { createInfo.baseWidth, createInfo.baseHeight, createInfo.baseDepth };
        for(std::uint8_t mip = 1; mip < mipCount; mip++) {
            tempMipDataAllocator.clear();
            Carrot::Vector<glm::vec4>& uncompressedMipColors = uncompressedMips[mip];
            Carrot::Vector<std::uint8_t> uncompressedMipPixels;
            const VkExtent3D mipDimensions = Carrot::ImageFormats::computeMipDimensions(mip, createInfo.baseWidth, createInfo.baseHeight, createInfo.baseDepth, srcFormat);
            const std::size_t uncompressedMipByteSize = Carrot::ImageFormats::computeMipSize(mipDimensions.width, mipDimensions.height, mipDimensions.depth, srcFormat);
            uncompressedMipColors.resize(uncompressedMipByteSize);
            uncompressedMipPixels.resize(mipDimensions.width * mipDimensions.height * mipDimensions.depth * sizeof(std::uint8_t) * destComponentCount);

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
                        uncompressedMipColors[index] = color;
                        uncompressedMipPixels[index * destComponentCount + 0] = color.r * 255;
                        if(destComponentCount > 1) uncompressedMipPixels[index * destComponentCount + 1] = color.g * 255;
                        if(destComponentCount > 2) uncompressedMipPixels[index * destComponentCount + 2] = color.b * 255;
                        if(destComponentCount > 3) uncompressedMipPixels[index * destComponentCount + 3] = color.a * 255;
                    }
                }
            }

            result = encodeMip(mip, mipDimensions, uncompressedMipPixels);

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
