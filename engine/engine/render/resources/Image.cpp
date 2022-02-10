//
// Created by jglrxavpok on 28/11/2020.
//

#include <OpenEXRConfig.h>
#include <ImfRgbaFile.h>

#include "Image.h"
#include "engine/render/resources/Buffer.h"
#include "stb_image.h"
#include "core/io/Logging.hpp"
#include "core/io/FileFormats.h"
#include "engine/utils/Profiling.h"
#include "core/async/Executors.h"
#include "core/async/Coroutines.hpp"
#include "engine/task/TaskScheduler.h"
#include "engine/Engine.h"


Carrot::Image::Image(Carrot::VulkanDriver& driver, vk::Extent3D extent, vk::ImageUsageFlags usage, vk::Format format,
                     std::set<std::uint32_t> families, vk::ImageCreateFlags flags, vk::ImageType imageType, std::uint32_t layerCount):
        Carrot::DebugNameable(), driver(driver), size(extent), layerCount(layerCount), format(format), imageData(true) {
    vk::ImageCreateInfo createInfo{
        .flags = flags,
        .imageType = imageType,
        .format = format,
        .extent = extent,
        .mipLevels = 1,
        .arrayLayers = layerCount,
        .samples = vk::SampleCountFlagBits::e1,
        .usage = usage,
        .initialLayout = vk::ImageLayout::eUndefined,
    };

    auto& queueFamilies = driver.getQueueFamilies();
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
    imageData.asOwned.memory = driver.getLogicalDevice().allocateMemoryUnique(allocationInfo, driver.getAllocationCallbacks());

    // bind memory to image
    driver.getLogicalDevice().bindImageMemory(getVulkanImage(), getMemory(), 0);
}

Carrot::Image::Image(Carrot::VulkanDriver& driver, vk::Image toView, vk::Extent3D extent, vk::Format format, std::uint32_t layerCount)
: driver(driver), imageData(false), format(format), layerCount(layerCount), size(extent) {
    imageData.asView.vkImage = toView;
}

const vk::Image& Carrot::Image::getVulkanImage() const {
    if(imageData.ownsImage) {
        return *imageData.asOwned.vkImage;
    } else {
        return imageData.asView.vkImage;
    }
}

void Carrot::Image::stageUpload(std::span<uint8_t> data, uint32_t layer, uint32_t layerCount) {
    // create buffer holding data
    auto stagingBuffer = Carrot::Buffer(driver,
                                        static_cast<vk::DeviceSize>(data.size()),
                                        vk::BufferUsageFlagBits::eTransferSrc,
                                         vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                                        {driver.getQueueFamilies().transferFamily.value()});

    // fill buffer
    stagingBuffer.directUpload(data.data(), data.size());

    // prepare image for transfer
    transitionLayout(vk::Format::eR8G8B8A8Unorm, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
    // copy from staging buffer to image
    driver.performSingleTimeTransferCommands([&](vk::CommandBuffer &commands) {
        vk::BufferImageCopy region = {
                .bufferOffset = 0,
                .bufferRowLength = 0,
                .bufferImageHeight = 0,
                .imageSubresource = {
                        .aspectMask = vk::ImageAspectFlagBits::eColor,
                        .mipLevel = 0,
                        .baseArrayLayer = layer,
                        .layerCount = layerCount,
                },
                .imageExtent = size,
        };
        commands.copyBufferToImage(stagingBuffer.getVulkanBuffer(), getVulkanImage(),
                                   vk::ImageLayout::eTransferDstOptimal, region);
    });

    // prepare image for shader reads
    transitionLayout(vk::Format::eR8G8B8A8Unorm, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
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
        auto format = IO::getFileFormat(resource.getName().c_str());
        if(!IO::isImageFormat(format)) {
            throw std::runtime_error("Unsupported filetype: "+resource.getName());
        }

        // support for images not supported by stbi will go here
        if(format == IO::FileFormat::EXR) {
            using namespace OPENEXR_IMF_NAMESPACE;
            using namespace IMATH_NAMESPACE;

            Profiling::PrintingScopedTimer _timer("EXR file load");
            RgbaInputFile file (resource.getName().c_str());

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
                    .levelCount = 1,
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
        }, true, {}, static_cast<vk::PipelineStageFlagBits>(0));
    } else {
        driver.performSingleTimeTransferCommands([&](vk::CommandBuffer &commands) {
            transitionLayoutInline(commands, oldLayout, newLayout);
        }, true, {}, static_cast<vk::PipelineStageFlagBits>(0));
    }
}

vk::UniqueImageView Carrot::Image::createImageView(vk::Format imageFormat, vk::ImageAspectFlags aspect, vk::ImageViewType viewType, std::uint32_t layerCount) {
    return std::move(driver.createImageView(getVulkanImage(), imageFormat, aspect, viewType, layerCount));
}

void Carrot::Image::setDebugNames(const std::string& name) {
    nameSingle(driver, name, getVulkanImage());
    if(imageData.ownsImage) {
        nameSingle(driver, name + " Memory", getMemory());
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
                .task = Async::AsTask<void>([direction, &allSizes, &allPixels, &textureSupplier]() -> void {
                    auto index = static_cast<std::size_t>(direction);
                    int w, h, n;
                    stbi_uc* pixels = stbi_load(textureSupplier(direction).c_str(), &w, &h, &n, STBI_rgb_alpha);
                    allSizes[index] = { w, h };
                    allPixels[index] = pixels;
                }),
                .joiner = &waitForPixelReads,
        };
        GetTaskScheduler().schedule(std::move(task));
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
    image->stageUpload(std::span<uint8_t>{allPixels[static_cast<std::size_t>(Skybox::Direction::PositiveX)], imageSize}, 4);
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

const vk::DeviceMemory& Carrot::Image::getMemory() const {
    verify(imageData.ownsImage, "Cannot access memory of not-owned image.")
    return *imageData.asOwned.memory;
}
