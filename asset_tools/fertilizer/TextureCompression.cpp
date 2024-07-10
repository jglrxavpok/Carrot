//
// Created by jglrxavpok on 04/11/2022.
//
#include "TextureCompression.h"
#include "core/utils/stringmanip.h"
#include <ktx.h>
#include <vulkan/vulkan_core.h>

#include <stb_image.h>
#include <core/Macros.h>
#include <core/render/ImageFormats.h>

namespace Fertilizer {
    ConversionResult compressTexture(const std::filesystem::path& inputFile, const std::filesystem::path& outputFile) {
        int w, h, comp;
        stbi_uc* pixels = stbi_load(inputFile.string().c_str(), &w, &h, &comp, 0);
        std::size_t srcSize = w * h * comp;

        ktxTexture2* texture;
        ktxTextureCreateInfo createInfo;
        KTX_error_code result;
        ktx_uint32_t level, layer, faceSlice;

        createInfo.glInternalformat = 0;  //Ignored as we'll create a KTX2 texture.

        switch(comp) {
            case 1:
                createInfo.vkFormat = VK_FORMAT_R8_UNORM;
                break;
            case 2:
                createInfo.vkFormat = VK_FORMAT_R8G8_UNORM;
                break;
            case 3:
                createInfo.vkFormat = VK_FORMAT_R8G8B8_UNORM;
                break;
            case 4:
                createInfo.vkFormat = VK_FORMAT_R8G8B8A8_UNORM;
                break;
            default:
                return {
                    .errorCode = ConversionResultError::UnsupportedInputType,
                    .errorMessage = Carrot::sprintf("Unsupported channel count: %d", comp),
                };
        }
        createInfo.baseWidth = w;
        createInfo.baseHeight = h;
        createInfo.baseDepth = 1;
        createInfo.numDimensions = 2;
        createInfo.numLevels = 1;
        createInfo.numLayers = 1;
        createInfo.numFaces = 1;
        createInfo.isArray = KTX_FALSE;
        createInfo.generateMipmaps = KTX_TRUE;

        result = ktxTexture2_Create(&createInfo,
                                    KTX_TEXTURE_CREATE_ALLOC_STORAGE,
                                    &texture);

        if(result != ktx_error_code_e::KTX_SUCCESS) {
            return {
                    .errorCode = ConversionResultError::TextureCompressionError,
                    .errorMessage = ktxErrorString(result),
            };
        }

        level = 0;
        layer = 0;
        faceSlice = 0;
        result = ktxTexture_SetImageFromMemory(ktxTexture(texture),
                                               level, layer, faceSlice,
                                               pixels, srcSize);

        stbi_image_free(pixels);

        if(result != ktx_error_code_e::KTX_SUCCESS) {
            return {
                .errorCode = ConversionResultError::TextureCompressionError,
                .errorMessage = ktxErrorString(result),
            };
        }
// Repeat for the other 15 slices of the base level and all other levels
// up to createInfo.numLevels.

        // For UASTC
        ktxBasisParams params = {0};
        params.structSize = sizeof(params);
        params.uastc = KTX_TRUE;
        // Set other BasisLZ/ETC1S or UASTC params to change default quality settings.
        result = ktxTexture2_CompressBasisEx(texture, &params);

        // Assume all GPUs targeted by Carrot support BC compression (they better)
        ktx_transcode_fmt_e targetFormat = KTX_TTF_BC3_RGBA;

        result = ktxTexture2_TranscodeBasis(texture, targetFormat, 0);
        if(result != ktx_error_code_e::KTX_SUCCESS) {
            return ConversionResult {
                .errorCode = ConversionResultError::TextureCompressionError,
                .errorMessage = ktxErrorString(result),
            };
        }

        std::uint8_t mipCount = Carrot::ImageFormats::computeMipCount(createInfo.baseWidth, createInfo.baseWidth, createInfo.baseDepth, static_cast<VkFormat>(createInfo.vkFormat));
        for(std::uint8_t mip = 1; mip < mipCount; mip++) {
            //TODO;
        }

        // TODO: create mips

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
