//
// Created by jglrxavpok on 28/11/2020.
//

//#define EXR_SUPPORT
#ifdef EXR_SUPPORT
#include <OpenEXRConfig.h>
#include <ImfRgbaFile.h>
#endif
#include <ktx.h>

#include "Image.h"

#include <core/render/ImageFormats.h>
#include <engine/render/resources/Buffer.h>
#include <engine/vulkan/VulkanDriver.h>
#include "stb_image.h"
#include <core/io/Logging.hpp>
#include <core/io/FileFormats.h>
#include <engine/utils/Profiling.h>
#include <core/async/Coroutines.hpp>
#include <engine/task/TaskScheduler.h>
#include <engine/render/resources/ResourceAllocator.h>

/*static*/ Carrot::Async::SpinLock Carrot::Image::AliveImagesAccess{};
/*static*/ std::unordered_set<const Carrot::Image*> Carrot::Image::AliveImages{};

Carrot::Image::Image(Carrot::VulkanDriver& driver, vk::Extent3D extent, vk::ImageUsageFlags usage, vk::Format format,
                     std::set<std::uint32_t> families, vk::ImageCreateFlags flags, vk::ImageType imageType, std::uint32_t layerCount, std::uint32_t mipCount):
        Carrot::DebugNameable(), driver(driver), size(extent), layerCount(layerCount), mipCount(mipCount), usage(usage), format(format), imageData(true) {
    vk::ImageCreateInfo createInfo{
        .flags = flags,
        .imageType = imageType,
        .format = format,
        .extent = extent,
        .mipLevels = mipCount,
        .arrayLayers = layerCount,
        .samples = vk::SampleCountFlagBits::e1,
        .usage = usage,
        .initialLayout = vk::ImageLayout::eUndefined,
    };

    auto& queueFamilies = driver.getQueuePartitioning();
    if(families.empty()) {
        families.insert(queueFamilies.graphicsFamily.value());
    }

    std::vector<uint32_t> familyList = {families.begin(), families.end()};

    if(families.size() == 1) { // same queue for graphics and transfer
        createInfo.sharingMode = vk::SharingMode::eExclusive; // used by only one queue
    } else { // separate queues, requires to tell Vulkan which queues
        createInfo.sharingMode = vk::SharingMode::eConcurrent; // used by both transfer and graphics queues
        createInfo.queueFamilyIndexCount = static_cast<uint32_t>(familyList.size());
        createInfo.pQueueFamilyIndices = familyList.data();
    }
    imageData.asOwned.vkImage = driver.getLogicalDevice().createImageUnique(createInfo, driver.getAllocationCallbacks());

    // allocate memory to use image
    vk::MemoryRequirements requirements = driver.getLogicalDevice().getImageMemoryRequirements(getVulkanImage());
    vk::MemoryAllocateInfo allocationInfo{
        .allocationSize = requirements.size,
        .memoryTypeIndex = driver.findMemoryType(requirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal),
    };

    imageData.asOwned.memory = std::move(Carrot::DeviceMemory(allocationInfo));

    // bind memory to image
    driver.getLogicalDevice().bindImageMemory(getVulkanImage(), getVkMemory(), 0);

    Async::LockGuard g { AliveImagesAccess };
    AliveImages.insert(this);
}

Carrot::Image::Image(Carrot::VulkanDriver& driver, vk::Image toView, vk::Extent3D extent, vk::Format format, std::uint32_t layerCount, std::uint32_t mipCount)
: driver(driver), imageData(false), format(format), layerCount(layerCount), mipCount(mipCount), size(extent) {
    imageData.asView.vkImage = toView;
}

Carrot::Image::~Image() noexcept {
    if(imageData.ownsImage) {
        GetVulkanDriver().deferDestroy("-memory", std::move(imageData.asOwned.memory));
        GetVulkanDriver().deferDestroy("-image", std::move(imageData.asOwned.vkImage));

        Async::LockGuard g { AliveImagesAccess };
        AliveImages.erase(this);
    }
}

bool Carrot::Image::isOwned() const {
    return imageData.ownsImage;
}

const vk::Image& Carrot::Image::getVulkanImage() const {
    if(imageData.ownsImage) {
        return *imageData.asOwned.vkImage;
    } else {
        return imageData.asView.vkImage;
    }
}

void Carrot::Image::stageUpload(Carrot::BufferView textureData, std::uint32_t layer, std::uint32_t layerCount, std::uint32_t startMip, std::uint32_t mipCount) {
    verify(startMip < mipCount, "Start mip must be < mipCount");
    verify(startMip+mipCount <= mipCount, "Mip range must be fully inside [0; mipCount]");
    // prepare image for transfer
    transitionLayout(format, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
    // copy from staging buffer to image
    driver.performSingleTimeTransferCommands([&](vk::CommandBuffer &commands) {
        Carrot::Vector<vk::BufferImageCopy> regions;
        regions.resize(mipCount);
        std::uint32_t mipOffset = 0;
        for(std::uint32_t mipIndex = 0; mipIndex < mipCount; mipIndex++) {
            std::uint32_t mipLevel = mipIndex + startMip;
            VkExtent3D mipSize = ImageFormats::computeMipDimensions(mipLevel, size.width, size.height, size.depth, static_cast<VkFormat>(format));
            vk::BufferImageCopy region = {
                    .bufferOffset = textureData.getStart() + mipOffset,
                    .bufferRowLength = 0,
                    .bufferImageHeight = 0,
                    .imageSubresource = {
                            .aspectMask = vk::ImageAspectFlagBits::eColor,
                            .mipLevel = mipLevel,
                            .baseArrayLayer = layer,
                            .layerCount = layerCount,
                    },
                    .imageExtent = { .width = mipSize.width, .height = mipSize.height, .depth = mipSize.depth }
            };
            regions[mipIndex] = region;
            mipOffset += computeMipDataSize(mipLevel);
        }
        commands.copyBufferToImage(textureData.getVulkanBuffer(), getVulkanImage(),
                                   vk::ImageLayout::eTransferDstOptimal, regions.size(), regions.data());
    });

    // prepare image for shader reads
    transitionLayout(format, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
}

void Carrot::Image::stageUpload(std::span<uint8_t> data, uint32_t layer, uint32_t layerCount) {
    verify(usage & vk::ImageUsageFlagBits::eTransferDst, "Cannot transfer to this image!");
    // create buffer holding data
    auto stagingBuffer = Carrot::Buffer(driver,
                                        static_cast<vk::DeviceSize>(data.size_bytes()),
                                        vk::BufferUsageFlagBits::eTransferSrc,
                                         vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                                        {driver.getQueuePartitioning().transferFamily.value()});

    stagingBuffer.setDebugNames("Image upload staging");

    // fill buffer
    stagingBuffer.directUpload(data.data(), data.size());

    stageUpload(stagingBuffer.getWholeView(), layer, layerCount, 0, 1);
}

std::unique_ptr<Carrot::Image> Carrot::Image::fromFile(Carrot::VulkanDriver& device, const Carrot::IO::Resource resource) {
    int width;
    int height;
    int channels;

    auto loadThroughStbi = [&]() {
        auto buffer = resource.readAll();
        stbi_uc* pixels = stbi_load_from_memory(buffer.get(), resource.getSize(), &width, &height, &channels, STBI_rgb_alpha);
        if(!pixels) {
            throw std::runtime_error("Failed to load image "+resource.getName());
        }

        auto image = std::make_unique<Carrot::Image>(device,
                                                     vk::Extent3D {
                                                             .width = static_cast<uint32_t>(width),
                                                             .height = static_cast<uint32_t>(height),
                                                             .depth = 1,
                                                     },
                                                     vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled/*TODO: customizable*/,
                                                     vk::Format::eR8G8B8A8Unorm);

        image->stageUpload(std::span<std::uint8_t>{pixels, static_cast<std::size_t>((width*height)*4) });
        image->name(resource.getName());

        stbi_image_free(pixels);

        return std::move(image);
    };

    if(resource.isFile()) {
        auto format = IO::getFileFormat(Carrot::toString(resource.getFilepath().u8string()).c_str());
        if(!IO::isImageFormat(format)) {
            throw std::runtime_error("Unsupported filetype: "+resource.getName());
        }

        // support for images not supported by stbi will go here
        if(format == IO::FileFormat::EXR) {
#ifdef EXR_SUPPORT
            using namespace OPENEXR_IMF_NAMESPACE;
            using namespace IMATH_NAMESPACE;

            Profiling::PrintingScopedTimer _timer("EXR file load");

            std::filesystem::path realPath = resource.getFilepath();
            RgbaInputFile file (Carrot::toString(realPath.u8string()).c_str());

            Box2i dw = file.dataWindow();
            width = dw.max.x - dw.min.x + 1;
            height = dw.max.y - dw.min.y + 1;

            std::size_t bufferSize = width * height;
            std::unique_ptr<Rgba[]> pixelBuffer = nullptr;

            {
                Profiling::PrintingScopedTimer _timer("EXR allocate memory");
                pixelBuffer = std::make_unique<Rgba[]>(bufferSize);
            }

            {
                Profiling::PrintingScopedTimer _timer("EXR file read pixels");
                {
                    Profiling::PrintingScopedTimer _timer("EXR file set framebuffer");
                    file.setFrameBuffer (pixelBuffer.get() - dw.min.x - dw.min.y * width, 1, width);
                }

                unsigned int coreCount = std::max(1u, std::thread::hardware_concurrency());
                setGlobalThreadCount(coreCount);
                file.readPixels (dw.min.y, dw.max.y);
            }

            auto image = std::make_unique<Carrot::Image>(device,
                                                         vk::Extent3D {
                                                                 .width = static_cast<uint32_t>(width),
                                                                 .height = static_cast<uint32_t>(height),
                                                                 .depth = 1,
                                                         },
                                                         vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled/*TODO: customizable*/,
                                                         vk::Format::eR16G16B16A16Sfloat);

            image->stageUpload(std::span<std::uint8_t>{reinterpret_cast<std::uint8_t*>(pixelBuffer.get()), bufferSize * 4 * sizeof(half) });
            image->name(resource.getName());

            return std::move(image);
#else
            throw std::runtime_error("EXR not supported (disabled at compilation)");
#endif
        } else if(format == IO::FileFormat::KTX2) {
            ktxTexture2* texture;
            KTX_error_code result;
            {
                std::unique_ptr<std::uint8_t[]> ktxData = resource.readAll();
                std::size_t ktxDataSize = resource.getSize();

                result = ktxTexture2_CreateFromMemory(ktxData.get(), ktxDataSize,
                                                     KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
                                                     &texture);

                if(result != ktx_error_code_e::KTX_SUCCESS) {
                    throw std::runtime_error("Could not read KTX2 file: "+resource.getName() + " error is " +
                                                     ktxErrorString(result));
                }

                vk::Format vkFormat = static_cast<vk::Format>(texture->vkFormat);

                if (ktxTexture2_NeedsTranscoding(texture)) {
                    ktx_texture_transcode_fmt_e tf;

                    const vk::PhysicalDeviceFeatures& deviceFeatures = GetVulkanDriver().getPhysicalDeviceFeatures();
                    if (deviceFeatures.textureCompressionETC2) {
                        tf = KTX_TTF_ETC2_RGBA;
                        vkFormat = vk::Format::eEtc2R8G8B8A8UnormBlock;
                    } else if (deviceFeatures.textureCompressionBC) {
                        tf = KTX_TTF_BC3_RGBA;
                        vkFormat = vk::Format::eBc3UnormBlock;
                    } else {
                        throw std::runtime_error("Vulkan implementation does not support any available transcode target.");
                    }

                    result = ktxTexture2_TranscodeBasis(texture, tf, 0);
                    if(result != ktx_error_code_e::KTX_SUCCESS) {
                        throw std::runtime_error("Could not transcode KTX2 file: "+resource.getName() + " error is " +
                                                 ktxErrorString(result));
                    }
                }

                ktxTexture* pTexture = ktxTexture(texture);
                std::uint32_t mipCount = pTexture->numLevels;
                vk::ImageType imageType = vk::ImageType::e2D;
                std::uint32_t faceCount = pTexture->numFaces;
                vk::ImageCreateFlags flags = static_cast<vk::ImageCreateFlags>(0);
                verify(faceCount == 1 || faceCount == 6, "Carrot currently only supports 2D or cubemap textures.");
                if(faceCount == 6) {
                    flags |= vk::ImageCreateFlagBits::eCubeCompatible;
                }
                auto image = std::make_unique<Carrot::Image>(device,
                                                             vk::Extent3D {
                                                                     .width = texture->baseWidth,
                                                                     .height = texture->baseHeight,
                                                                     .depth = texture->baseDepth,
                                                             },
                                                             vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled/*TODO: customizable*/,
                                                             vkFormat,
                                                             std::set<uint32_t>{},
                                                             flags,
                                                             imageType,
                                                             pTexture->numFaces * pTexture->numLayers,
                                                             mipCount
                                                             );

                std::size_t totalSize = 0;
                for(std::uint32_t mipIndex = 0; mipIndex < mipCount; mipIndex++) {
                    std::size_t expectedMipSize = image->computeMipDataSize(mipIndex);
                    std::size_t ktxImageSize = ktxTexture_GetImageSize(pTexture, mipIndex);
                    verify(ktxImageSize == expectedMipSize, "mip size in ktx and expected by engine are different! Probably a programming error");
                    totalSize += expectedMipSize;
                }
                totalSize *= faceCount;

                // vkCmdCopyBufferToImage needs the buffer offset to be a multiple of the block size, so align the data properly
                Carrot::BufferAllocation stagingData = GetResourceAllocator().allocateStagingBuffer(totalSize, ImageFormats::getBlockSize(static_cast<VkFormat>(vkFormat)));
                std::uint8_t* pImageData = stagingData.view.map<std::uint8_t>();
                std::size_t destinationOffset = 0;
                for(std::uint32_t mipIndex = 0; mipIndex < mipCount; mipIndex++) {
                    for(std::uint32_t faceIndex = 0; faceIndex < faceCount; faceIndex++) {
                        ktx_size_t offset;
                        result = ktxTexture_GetImageOffset(pTexture, mipIndex, 0, faceIndex, &offset);
                        if(result != ktx_error_code_e::KTX_SUCCESS) {
                            throw std::runtime_error(resource.getName() + ", ktxTexture_GetImageOffset error is " +
                                                     ktxErrorString(result));
                        }

                        const std::size_t mipSize = ktxTexture_GetImageSize(pTexture, mipIndex);
                        std::uint8_t* mipPixelData = ktxTexture_GetData(pTexture) + offset;
                        std::uint8_t* pDestination = pImageData + destinationOffset;
                        memcpy(pDestination, mipPixelData, mipSize);

                        destinationOffset += image->computeMipDataSize(mipIndex);
                    }
                }

                image->stageUpload(stagingData.view, 0, faceCount, 0, mipCount);
                image->name(resource.getName());

                ktxTexture_Destroy(pTexture);

                return image;
            }
        } else {
            return loadThroughStbi();
        }
    } else {
        return loadThroughStbi();
    }
}

const vk::Extent3D& Carrot::Image::getSize() const {
    return size;
}

void Carrot::Image::transitionLayoutInline(vk::CommandBuffer& commands,vk::ImageLayout oldLayout, vk::ImageLayout newLayout, vk::ImageAspectFlags aspect) {
    transition(getVulkanImage(), commands, oldLayout, newLayout, layerCount, aspect);
}

void Carrot::Image::transition(vk::Image image, vk::CommandBuffer& commands, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, uint32_t layerCount, vk::ImageAspectFlags aspect) {
    vk::ImageMemoryBarrier barrier {
            .oldLayout = oldLayout,
            .newLayout = newLayout,

            // concurrent sharing, so no ownership transfer
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,

            .image = image,

            .subresourceRange = {
                    .aspectMask = aspect,
                    .baseMipLevel = 0,
                    .levelCount = VK_REMAINING_MIP_LEVELS,
                    .baseArrayLayer = 0,
                    .layerCount = static_cast<uint32_t>(layerCount),
            }
    };

    auto sourceStage = static_cast<vk::PipelineStageFlags>(0);
    auto destinationStage = static_cast<vk::PipelineStageFlags>(0);

    bool oldLayoutHandled = true;
    bool newLayoutHandled = true;
    switch(oldLayout) {
        case vk::ImageLayout::eUndefined: {
            barrier.srcAccessMask = static_cast<vk::AccessFlagBits>(0);
            sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
        } break;

        case vk::ImageLayout::eGeneral: {
            barrier.srcAccessMask = vk::AccessFlagBits::eMemoryWrite;
            sourceStage = vk::PipelineStageFlagBits::eRayTracingShaderKHR;
        } break;

        case vk::ImageLayout::eTransferDstOptimal: {
            barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite; // write must be done
            sourceStage = vk::PipelineStageFlagBits::eTransfer;
        } break;

        case vk::ImageLayout::eTransferSrcOptimal: {
            barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead; // transfer must be able to read
            sourceStage = vk::PipelineStageFlagBits::eTransfer;
        } break;

        case vk::ImageLayout::eDepthStencilAttachmentOptimal:
        case vk::ImageLayout::eDepthAttachmentOptimal:
        case vk::ImageLayout::eStencilAttachmentOptimal:
        case vk::ImageLayout::eDepthReadOnlyOptimal:
        case vk::ImageLayout::eDepthStencilReadOnlyOptimal:
        case vk::ImageLayout::eShaderReadOnlyOptimal:
        case vk::ImageLayout::eColorAttachmentOptimal: {
            barrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite; // write must be done
            sourceStage = vk::PipelineStageFlagBits::eFragmentShader;
        } break;

        default:
            oldLayoutHandled = false;
            break;
    }

    switch(newLayout) {
        case vk::ImageLayout::eTransferDstOptimal: {
            barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite; // write must be done
            destinationStage = vk::PipelineStageFlagBits::eTransfer;
        } break;

        case vk::ImageLayout::eShaderReadOnlyOptimal: {
            barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead; // shader must be able to read
            destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
        } break;
        case vk::ImageLayout::eColorAttachmentOptimal: {
            barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite; // shader must be able to read
            destinationStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        } break;

        case vk::ImageLayout::eGeneral: {
            barrier.dstAccessMask = vk::AccessFlagBits::eMemoryWrite; // shader must be able to read
            destinationStage = vk::PipelineStageFlagBits::eAllCommands;
        } break;

        case vk::ImageLayout::eTransferSrcOptimal: {
            barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead; // shader must be able to read
            destinationStage = vk::PipelineStageFlagBits::eTransfer;
        } break;

        case vk::ImageLayout::ePresentSrcKHR: {
            barrier.dstAccessMask = vk::AccessFlagBits::eMemoryRead; // shader must be able to read
            destinationStage = vk::PipelineStageFlagBits::eTransfer;
        } break;

        case vk::ImageLayout::eDepthAttachmentOptimal:
        case vk::ImageLayout::eDepthReadOnlyOptimal:
        case vk::ImageLayout::eDepthStencilAttachmentOptimal:
        case vk::ImageLayout::eDepthStencilReadOnlyOptimal: {
            barrier.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead; // shader must be able to read
            destinationStage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
        } break;

        default:
            newLayoutHandled = false;
            break;
    }

#if 0
    assert(oldLayoutHandled && newLayoutHandled);
#else
    if(!oldLayoutHandled) {
        Carrot::Log::error("Unhandled old layout: %s", to_string(oldLayout).c_str());
    }
    if(!newLayoutHandled) {
        Carrot::Log::error("Unhandled new layout: %s", to_string(newLayout).c_str());
    }
#endif

    commands.pipelineBarrier(sourceStage,
                             destinationStage,
                             static_cast<vk::DependencyFlags>(0),
                             {},
                             {},
                             barrier
    );
}

void Carrot::Image::transitionLayout(vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout) {
    bool onGraphicsQueue = false;

    // ensure we are using the correct command pool and queue if transitioning to graphics-related layouts
    if(newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        onGraphicsQueue = true;
    }
    if(onGraphicsQueue) {
        driver.performSingleTimeGraphicsCommands([&](vk::CommandBuffer &commands) {
            transitionLayoutInline(commands, oldLayout, newLayout);
        }, true, {}, static_cast<vk::PipelineStageFlagBits2>(0));
    } else {
        driver.performSingleTimeTransferCommands([&](vk::CommandBuffer &commands) {
            transitionLayoutInline(commands, oldLayout, newLayout);
        }, true, {}, static_cast<vk::PipelineStageFlagBits2>(0));
    }
}

vk::UniqueImageView Carrot::Image::createImageView(vk::Format imageFormat, vk::ImageAspectFlags aspectMask, vk::ImageViewType viewType, std::uint32_t layerCount) {
    return driver.getLogicalDevice().createImageViewUnique({
                                                            .image = getVulkanImage(),
                                                            .viewType = viewType,
                                                            .format = imageFormat,

                                                            .components = {
                                                                    .r = vk::ComponentSwizzle::eIdentity,
                                                                    .g = vk::ComponentSwizzle::eIdentity,
                                                                    .b = vk::ComponentSwizzle::eIdentity,
                                                                    .a = vk::ComponentSwizzle::eIdentity,
                                                            },

                                                            .subresourceRange = {
                                                                    .aspectMask = aspectMask,
                                                                    .baseMipLevel = 0,
                                                                    .levelCount = VK_REMAINING_MIP_LEVELS,
                                                                    .baseArrayLayer = 0,
                                                                    .layerCount = layerCount,
                                                            },
                                                    }, driver.getAllocationCallbacks());
}

void Carrot::Image::setDebugNames(const std::string& name) {
    nameSingle(name, getVulkanImage());
    if(imageData.ownsImage) {
        imageData.asOwned.memory.name(name);
        nameSingle(name + " Memory", getVkMemory());
    }
}

std::unique_ptr<Carrot::Image> Carrot::Image::cubemapFromFiles(Carrot::VulkanDriver& device, std::function<std::string(Skybox::Direction)> textureSupplier) {
    std::array<stbi_uc*, 6> allPixels {
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
    };
    std::array<glm::ivec2, 6> allSizes {
            glm::ivec2 {-1,-1},
            glm::ivec2 {-1,-1},
            glm::ivec2 {-1,-1},
            glm::ivec2 {-1,-1},
            glm::ivec2 {-1,-1},
            glm::ivec2 {-1,-1},
    };

    Async::Counter waitForPixelReads;
    for(Skybox::Direction direction : { Skybox::Direction::NegativeX, Skybox::Direction::PositiveX, Skybox::Direction::NegativeY, Skybox::Direction::PositiveY, Skybox::Direction::NegativeZ, Skybox::Direction::PositiveZ }) {
        std::string textureName = textureSupplier(direction);

        Carrot::TaskDescription task {
                .name = Carrot::sprintf("Read cubemap face %s", textureName.c_str()),
                .task = [direction, &allSizes, &allPixels, &textureSupplier](TaskHandle& task) -> void {
                    auto index = static_cast<std::size_t>(direction);
                    int w, h, n;
                    stbi_uc* pixels = stbi_load(textureSupplier(direction).c_str(), &w, &h, &n, STBI_rgb_alpha);
                    allSizes[index] = { w, h };
                    allPixels[index] = pixels;
                },
                .joiner = &waitForPixelReads,
        };
        GetTaskScheduler().schedule(std::move(task), TaskScheduler::AssetLoading);
    }
    waitForPixelReads.sleepWait();

    int firstWidth = allSizes[0].x;
    int firstHeight = allSizes[0].y;
    verify(allPixels[0] != nullptr, "Failed to load for direction 0");
    for (int i = 1; i < 6; i++) {
        int w = allSizes[i].x;
        int h = allSizes[i].y;
        verify(w == firstWidth && h == firstHeight, Carrot::sprintf("All face dimensions must match! Here expected %d x %d, got %d x %d", firstWidth, firstHeight, w, h));
        verify(allPixels[i] != nullptr, Carrot::sprintf("Failed to load for direction %d", i));
    }

    int w = firstWidth;
    int h = firstHeight;

    vk::Extent3D extent {
        .width = static_cast<uint32_t>(w),
        .height = static_cast<uint32_t>(h),
        .depth = 1,
    };
    vk::DeviceSize imageSize = w*h*4;
    auto image = std::make_unique<Image>(device,
                                    extent,
                                    vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
                                    vk::Format::eR8G8B8A8Unorm,
                                    device.createGraphicsAndTransferFamiliesSet(),
                                    vk::ImageCreateFlagBits::eCubeCompatible,
                                    vk::ImageType::e2D,
                                    6);

    image->stageUpload(std::span<uint8_t>{allPixels[static_cast<std::size_t>(Skybox::Direction::PositiveX)], imageSize}, 0);
    image->stageUpload(std::span<uint8_t>{allPixels[static_cast<std::size_t>(Skybox::Direction::NegativeX)], imageSize}, 1);
    image->stageUpload(std::span<uint8_t>{allPixels[static_cast<std::size_t>(Skybox::Direction::PositiveY)], imageSize}, 2);
    image->stageUpload(std::span<uint8_t>{allPixels[static_cast<std::size_t>(Skybox::Direction::NegativeY)], imageSize}, 3);
    image->stageUpload(std::span<uint8_t>{allPixels[static_cast<std::size_t>(Skybox::Direction::PositiveZ)], imageSize}, 4);
    image->stageUpload(std::span<uint8_t>{allPixels[static_cast<std::size_t>(Skybox::Direction::NegativeZ)], imageSize}, 5);

    stbi_image_free(allPixels[static_cast<std::size_t>(Skybox::Direction::PositiveX)]);
    stbi_image_free(allPixels[static_cast<std::size_t>(Skybox::Direction::NegativeX)]);
    stbi_image_free(allPixels[static_cast<std::size_t>(Skybox::Direction::PositiveY)]);
    stbi_image_free(allPixels[static_cast<std::size_t>(Skybox::Direction::NegativeY)]);
    stbi_image_free(allPixels[static_cast<std::size_t>(Skybox::Direction::PositiveZ)]);
    stbi_image_free(allPixels[static_cast<std::size_t>(Skybox::Direction::NegativeZ)]);

    return image;
}

vk::Format Carrot::Image::getFormat() const {
    return format;
}

vk::DeviceMemory Carrot::Image::getVkMemory() const {
    verify(imageData.ownsImage, "Cannot access memory of not-owned image.")
    return imageData.asOwned.memory.getVulkanMemory();
}

const Carrot::DeviceMemory& Carrot::Image::getMemory() const {
    verify(imageData.ownsImage, "Cannot access memory of not-owned image.")
    return imageData.asOwned.memory;
}

std::size_t Carrot::Image::computeMipDataSize(std::uint32_t mipLevel) const {
    return ImageFormats::computeMipSize(mipLevel, size.width, size.height, size.depth, static_cast<VkFormat>(format));
}
