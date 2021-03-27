//
// Created by jglrxavpok on 28/11/2020.
//

#include "Image.h"
#include "engine/render/resources/Buffer.h"
#include "stb_image.h"

Carrot::Image::Image(Carrot::VulkanDriver& driver, vk::Extent3D extent, vk::ImageUsageFlags usage, vk::Format format,
                     set<uint32_t> families, vk::ImageCreateFlags flags, vk::ImageType imageType, uint32_t layerCount):
        Carrot::DebugNameable(), driver(driver), size(extent), layerCount(layerCount) {
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
    vkImage = driver.getLogicalDevice().createImageUnique(createInfo, driver.getAllocationCallbacks());

    // allocate memory to use image
    vk::MemoryRequirements requirements = driver.getLogicalDevice().getImageMemoryRequirements(*vkImage);
    vk::MemoryAllocateInfo allocationInfo{
        .allocationSize = requirements.size,
        .memoryTypeIndex = driver.findMemoryType(requirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal),
    };
    memory = driver.getLogicalDevice().allocateMemoryUnique(allocationInfo, driver.getAllocationCallbacks());

    // bind memory to image
    driver.getLogicalDevice().bindImageMemory(*vkImage, *memory, 0);
}

const vk::Image& Carrot::Image::getVulkanImage() const {
    return *vkImage;
}

void Carrot::Image::stageUpload(const vector<uint8_t> &data, uint32_t layer, uint32_t layerCount) {
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
        commands.copyBufferToImage(stagingBuffer.getVulkanBuffer(), *vkImage,
                                   vk::ImageLayout::eTransferDstOptimal, region);
    });

    // prepare image for shader reads
    transitionLayout(vk::Format::eR8G8B8A8Unorm, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
}

std::unique_ptr<Carrot::Image> Carrot::Image::fromFile(Carrot::VulkanDriver& device, const string &filename) {
    int width;
    int height;
    int channels;
    stbi_uc* pixels = stbi_load(filename.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if(!pixels) {
        throw runtime_error("Failed to load image "+filename);
    }

    auto image = make_unique<Carrot::Image>(device,
                                        vk::Extent3D {
                                            .width = static_cast<uint32_t>(width),
                                            .height = static_cast<uint32_t>(height),
                                            .depth = 1,
                                        },
                                        vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled/*TODO: customizable*/,
                                        vk::Format::eR8G8B8A8Unorm);

    vector<uint8_t> pixelVector{pixels, pixels+(width*height)*4};
    image->stageUpload(pixelVector);

    stbi_image_free(pixels);

    return std::move(image);
}

const vk::Extent3D& Carrot::Image::getSize() const {
    return size;
}

void Carrot::Image::transitionLayoutInline(vk::CommandBuffer& commands, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout) {
    transition(*vkImage, commands, format, oldLayout, newLayout, layerCount);
}

void Carrot::Image::transition(vk::Image image, vk::CommandBuffer& commands, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, uint32_t layerCount) {
    vk::ImageMemoryBarrier barrier {
            .oldLayout = oldLayout,
            .newLayout = newLayout,

            // concurrent sharing, so no ownership transfer
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,

            .image = image,

            .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = static_cast<uint32_t>(layerCount),
            }
    };

    auto sourceStage = static_cast<vk::PipelineStageFlags>(0);
    auto destinationStage = static_cast<vk::PipelineStageFlags>(0);

    if(oldLayout == vk::ImageLayout::eUndefined) {
        barrier.srcAccessMask = static_cast<vk::AccessFlagBits>(0);
        sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
    }

    if(oldLayout == vk::ImageLayout::eGeneral) {
        barrier.srcAccessMask = vk::AccessFlagBits::eMemoryWrite;
        sourceStage = vk::PipelineStageFlagBits::eRayTracingShaderKHR;
    }

    if(oldLayout == vk::ImageLayout::eTransferDstOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite; // write must be done
        sourceStage = vk::PipelineStageFlagBits::eTransfer;
    }
    if(oldLayout == vk::ImageLayout::eColorAttachmentOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite; // write must be done
        sourceStage = vk::PipelineStageFlagBits::eFragmentShader;
    }
    if(oldLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite; // write must be done
        sourceStage = vk::PipelineStageFlagBits::eFragmentShader;
    }

    if(newLayout == vk::ImageLayout::eTransferDstOptimal) {
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite; // write must be done
        destinationStage = vk::PipelineStageFlagBits::eTransfer;
    }

    if(newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead; // shader must be able to read
        destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
    }
    if(newLayout == vk::ImageLayout::eColorAttachmentOptimal) {
        barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite; // shader must be able to read
        destinationStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    }

    if(newLayout == vk::ImageLayout::eGeneral) {
        barrier.dstAccessMask = vk::AccessFlagBits::eMemoryWrite; // shader must be able to read
        destinationStage = vk::PipelineStageFlagBits::eTopOfPipe;
    }

    commands.pipelineBarrier(sourceStage,
                             destinationStage,
                             static_cast<vk::DependencyFlags>(0),
                             {},
                             {},
                             barrier
    );
}

void Carrot::Image::transitionLayout(vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout) {
    vk::CommandPool commandPool = driver.getThreadTransferCommandPool();
    vk::Queue queue = driver.getTransferQueue();

    // ensure we are using the correct command pool and queue if transitioning to graphics-related layouts
    if(newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        commandPool = driver.getThreadGraphicsCommandPool();
        queue = driver.getGraphicsQueue();
    }
    driver.performSingleTimeCommands(commandPool, queue, [&](vk::CommandBuffer &commands) {
        transitionLayoutInline(commands, format, oldLayout, newLayout);
    });
}

vk::UniqueImageView Carrot::Image::createImageView(vk::Format imageFormat, vk::ImageAspectFlags aspect) {
    return std::move(driver.createImageView(*vkImage, imageFormat, aspect));
}

void Carrot::Image::setDebugNames(const string& name) {
    nameSingle(driver, name, *vkImage);
    nameSingle(driver, name + " Memory", *memory);
}

std::unique_ptr<Carrot::Image> Carrot::Image::cubemapFromFiles(Carrot::VulkanDriver& device, std::function<std::string(Skybox::Direction)> textureSupplier) {
    int w, h, n;
    int firstWidth, firstHeight;
    stbi_uc* pixelsNX = stbi_load(textureSupplier(Skybox::Direction::NegativeX).c_str(), &w, &h, &n, STBI_rgb_alpha);
    firstWidth = w;
    firstHeight = h;
    stbi_uc* pixelsNY = stbi_load(textureSupplier(Skybox::Direction::NegativeY).c_str(), &w, &h, &n, STBI_rgb_alpha);
    assert(w == firstWidth && h == firstHeight);
    stbi_uc* pixelsNZ = stbi_load(textureSupplier(Skybox::Direction::NegativeZ).c_str(), &w, &h, &n, STBI_rgb_alpha);
    assert(w == firstWidth && h == firstHeight);
    stbi_uc* pixelsPX = stbi_load(textureSupplier(Skybox::Direction::PositiveX).c_str(), &w, &h, &n, STBI_rgb_alpha);
    assert(w == firstWidth && h == firstHeight);
    stbi_uc* pixelsPY = stbi_load(textureSupplier(Skybox::Direction::PositiveY).c_str(), &w, &h, &n, STBI_rgb_alpha);
    assert(w == firstWidth && h == firstHeight);
    stbi_uc* pixelsPZ = stbi_load(textureSupplier(Skybox::Direction::PositiveZ).c_str(), &w, &h, &n, STBI_rgb_alpha);
    assert(w == firstWidth && h == firstHeight);

    vk::Extent3D extent {
        .width = static_cast<uint32_t>(w),
        .height = static_cast<uint32_t>(h),
        .depth = 1,
    };
    vk::DeviceSize imageSize = w*h*4;
    auto image = make_unique<Image>(device,
                                    extent,
                                    vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
                                    vk::Format::eR8G8B8A8Unorm,
                                    device.createGraphicsAndTransferFamiliesSet(),
                                    vk::ImageCreateFlagBits::eCubeCompatible,
                                    vk::ImageType::e2D,
                                    6);

    image->stageUpload(vector<uint8_t>{pixelsPX, pixelsPX+imageSize}, 0);
    image->stageUpload(vector<uint8_t>{pixelsNX, pixelsNX+imageSize}, 1);
    image->stageUpload(vector<uint8_t>{pixelsPY, pixelsPY+imageSize}, 2);
    image->stageUpload(vector<uint8_t>{pixelsNY, pixelsNY+imageSize}, 3);
    image->stageUpload(vector<uint8_t>{pixelsPZ, pixelsPZ+imageSize}, 4);
    image->stageUpload(vector<uint8_t>{pixelsNZ, pixelsNZ+imageSize}, 5);

    stbi_image_free(pixelsPZ);
    stbi_image_free(pixelsPY);
    stbi_image_free(pixelsPX);
    stbi_image_free(pixelsNZ);
    stbi_image_free(pixelsNY);
    stbi_image_free(pixelsNX);

    return image;
}
