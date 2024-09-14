//
// Created by jglrxavpok on 10/09/2024.
//
// Heavily based on https://learnopengl.com/PBR/IBL/Diffuse-irradiance

#include "EnvironmentMapProcessing.h"
#include <stb_image.h>
#include <array>
#include <core/Macros.h>
#include <core/containers/Vector.hpp>
#include <glm/glm.hpp>

#include <ktx.h>
#include <glm/ext/matrix_transform.hpp>
#include <vulkan/vulkan_core.h>

namespace Fertilizer {

    constexpr glm::vec2 invAtan = glm::vec2(0.1591f, 0.3183f);

    static glm::vec2 toSphericalMap(const glm::vec3& cubeUV) {
        // difference from LearnOpenGL: - on X to flip the entire cubemap horizontally to match with source .hdr file (as seen in ImageViewer)
        glm::vec2 uv = glm::vec2(-glm::atan(cubeUV.z, cubeUV.x), glm::asin(cubeUV.y));
        uv *= invAtan;
        uv += 0.5f;
        return uv;
    }

    ConversionResult processEnvironmentMap(const std::filesystem::path& inputFile, const std::filesystem::path& outputFile) {
        int width = 0;
        int height = 0;
        int comp = 0;
        float* pixels = stbi_loadf(inputFile.string().c_str(), &width, &height, &comp, 0);
        if(!pixels) {
            return ConversionResult {
                .errorCode = ConversionResultError::EnvironmentMapError,
                .errorMessage = "Failed to read pixels from file"
            };
        }

        if(comp <= 2) {
            return ConversionResult {
                .errorCode = ConversionResultError::EnvironmentMapError,
                .errorMessage = "Expected an RGB image"
            };
        }

        CLEANUP(stbi_image_free(pixels));

        // 1. convert to cubemap
        int faceWidth = 1024;
        int faceHeight = 1024;

        ktxTexture2* texture;
        ktxTextureCreateInfo createInfo {};
        KTX_error_code result;

        createInfo.glInternalformat = 0;  //Ignored as we'll create a KTX2 texture.

        createInfo.baseWidth = faceWidth;
        createInfo.baseHeight = faceHeight;
        createInfo.baseDepth = 1;
        createInfo.numDimensions = 2;

        createInfo.numLevels = 1;
        createInfo.numLayers = 1;
        createInfo.numFaces = 6;
        createInfo.isArray = KTX_FALSE;
        createInfo.generateMipmaps = KTX_FALSE;
        createInfo.vkFormat = VK_FORMAT_R32G32B32A32_SFLOAT;

        result = ktxTexture2_Create(&createInfo,
                                    KTX_TEXTURE_CREATE_ALLOC_STORAGE,
                                    &texture);

        if(result != ktx_error_code_e::KTX_SUCCESS) {
            return {
                .errorCode = ConversionResultError::EnvironmentMapError,
                .errorMessage = ktxErrorString(result),
            };
        }

        // render each face: input texture is expected to be in equirectangular projection, convert to a cubemap
        Carrot::Vector<glm::vec4> faceColors;
        faceColors.resize(faceWidth * faceHeight);

        constexpr glm::vec3 up = glm::vec3(0, 1, 0);
        constexpr glm::vec3 right = glm::vec3(1, 0, 0);

        // follows KTX2 order, left handed, Y up, Z forward
        const std::array faceRotations = {
            // +X
            glm::rotate(glm::mat4{1.0f}, glm::half_pi<float>(), up),
            // -X
            glm::rotate(glm::mat4{1.0f}, -glm::half_pi<float>(), up),
            // +Y
            glm::rotate(glm::mat4{1.0f}, glm::half_pi<float>(), right),
            // -Y
            glm::rotate(glm::mat4{1.0f}, -glm::half_pi<float>(), right),
            // +Z
            glm::mat4{1.0f},
            // -Z
            glm::rotate(glm::mat4{1.0f}, glm::pi<float>(), up),
        };
        for(int face = 0; face < 6; face++) {
            for(int y = 0; y < faceHeight; y++) {
                for(int x = 0; x < faceWidth; x++) {
                    // sample environment map
                    glm::vec2 faceUV = glm::vec2(x / static_cast<float>(faceWidth), y / static_cast<float>(faceHeight)) *2.0f -1.0f;
                    glm::vec3 cubeUV = faceRotations[face] * glm::vec4(faceUV, 1, 0);

                    glm::vec2 equirectangularUV = toSphericalMap(glm::normalize(cubeUV));
                    int pixelX = glm::round(equirectangularUV.x * (width-1));
                    int pixelY = glm::round(equirectangularUV.y * (height-1));
                    faceColors[x + y * faceWidth] = glm::vec4 {
                        pixels[(pixelX + pixelY * width) * comp + 0],
                        pixels[(pixelX + pixelY * width) * comp + 1],
                        pixels[(pixelX + pixelY * width) * comp + 2],
                        1.0f,
                    };
                }
            }

            // write face to ktx2
            result = ktxTexture_SetImageFromMemory(ktxTexture(texture),
                                                       0, 0, face,
                                                       reinterpret_cast<const std::uint8_t*>(faceColors.cdata()), faceColors.bytes_size());
            if(result != ktx_error_code_e::KTX_SUCCESS) {
                return {
                    .errorCode = ConversionResultError::EnvironmentMapError,
                    .errorMessage = ktxErrorString(result),
                };
            }
        }

        result = ktxTexture_WriteToNamedFile(ktxTexture(texture), outputFile.string().c_str());
        ktxTexture_Destroy(ktxTexture(texture));

        return result == ktx_error_code_e::KTX_SUCCESS
        ? ConversionResult {
            .errorCode = ConversionResultError::Success,
        }
        : ConversionResult {
            .errorCode = ConversionResultError::EnvironmentMapError,
            .errorMessage = ktxErrorString(result),
        };

        // 3. TODO: create specular map
    }
}
