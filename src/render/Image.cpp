//
// Created by jglrxavpok on 28/11/2020.
//

#include "Image.h"
#include "Buffer.h"
#include "stb_image.h"

Carrot::Image::Image(Carrot::Engine& engine, vk::Extent3D extent, vk::ImageUsageFlags usage, vk::Format format, set<uint32_t> families, vk::ImageCreateFlags flags, vk::ImageType imageType):
engine(engine), size(extent) {
    auto& device = engine.getLogicalDevice();
    vk::ImageCreateInfo createInfo{
        .flags = flags,
        .imageType = imageType,
        .format = format,
        .extent = extent,
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = vk::SampleCountFlagBits::e1,
        .usage = usage,
        .initialLayout = vk::ImageLayout::eUndefined,
    };

    auto& queueFamilies = engine.getQueueFamilies();
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
    vkImage = device.createImageUnique(createInfo, engine.getAllocator());

    // allocate memory to use image
    vk::MemoryRequirements requirements = device.getImageMemoryRequirements(*vkImage);
    vk::MemoryAllocateInfo allocationInfo{
        .allocationSize = requirements.size,
        .memoryTypeIndex = engine.findMemoryType(requirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal),
    };
    memory = device.allocateMemoryUnique(allocationInfo, engine.getAllocator());

    // bind memory to image
    device.bindImageMemory(*vkImage, *memory, 0);
}

const vk::Image& Carrot::Image::getVulkanImage() const {
    return *vkImage;
}

void Carrot::Image::stageUpload(const vector<uint8_t> &data) {
    // create buffer holding data
    auto stagingBuffer = Carrot::Buffer(engine,
                                         static_cast<vk::DeviceSize>(data.size()),
                                         vk::BufferUsageFlagBits::eTransferSrc,
                                         vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                                         {engine.getQueueFamilies().transferFamily.value()});

    // fill buffer
    stagingBuffer.directUpload(data.data(), data.size());

    // prepare image for transfer
    transitionLayout(vk::Format::eR8G8B8A8Srgb, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
    // copy from staging buffer to image
    engine.performSingleTimeTransferCommands([&](vk::CommandBuffer &commands) {
        vk::BufferImageCopy region = {
                .bufferOffset = 0,
                .bufferRowLength = 0,
                .bufferImageHeight = 0,
                .imageSubresource = {
                        .aspectMask = vk::ImageAspectFlagBits::eColor,
                        .mipLevel = 0,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                },
                .imageExtent = size,
        };
        commands.copyBufferToImage(stagingBuffer.getVulkanBuffer(), *vkImage,
                                   vk::ImageLayout::eTransferDstOptimal, region);
    });

    // prepare image for shader reads
    transitionLayout(vk::Format::eR8G8B8A8Srgb, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
}

unique_ptr<Carrot::Image> Carrot::Image::fromFile(Carrot::Engine& engine, const string &filename) {
    int width;
    int height;
    int channels;
    stbi_uc* pixels = stbi_load(filename.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if(!pixels) {
        throw runtime_error("Failed to load image "+filename);
    }

    auto image = make_unique<Carrot::Image>(engine,
                                        vk::Extent3D {
                                            .width = static_cast<uint32_t>(width),
                                            .height = static_cast<uint32_t>(height),
                                            .depth = 1,
                                        },
                                        vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled/*TODO: customizable*/,
                                        vk::Format::eR8G8B8A8Srgb);

    vector<uint8_t> pixelVector{pixels, pixels+(width*height)*4};
    image->stageUpload(pixelVector);

    stbi_image_free(pixels);

    return std::move(image);
}

const vk::Extent3D& Carrot::Image::getSize() const {
    return size;
}

void Carrot::Image::transitionLayout(vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout) {
    vk::CommandPool commandPool = engine.getTransferCommandPool();
    vk::Queue queue = engine.getTransferQueue();

    // ensure we are using the correct command pool and queue if transitioning to graphics-related layouts
    if(newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        commandPool = engine.getGraphicsCommandPool();
        queue = engine.getGraphicsQueue();
    }
    engine.performSingleTimeCommands(commandPool, queue, [&](vk::CommandBuffer &commands) {
        vk::ImageMemoryBarrier barrier {
                .oldLayout = oldLayout,
                .newLayout = newLayout,

                // concurrent sharing, so no ownership transfer
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,

                .image = *vkImage,

                .subresourceRange = {
                        .aspectMask = vk::ImageAspectFlagBits::eColor,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                }
        };

        auto sourceStage = static_cast<vk::PipelineStageFlags>(0);
        auto destinationStage = static_cast<vk::PipelineStageFlags>(0);

        if(oldLayout == vk::ImageLayout::eUndefined) {
            barrier.srcAccessMask = static_cast<vk::AccessFlagBits>(0);
            sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
        }

        if(oldLayout == vk::ImageLayout::eTransferDstOptimal) {
            barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite; // write must be done
            sourceStage = vk::PipelineStageFlagBits::eTransfer;
        }
        if(newLayout == vk::ImageLayout::eTransferDstOptimal) {
            barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite; // write must be done
            destinationStage = vk::PipelineStageFlagBits::eTransfer;
        }

        if(newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
            barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead; // shader must be able to read
            destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
        }

        commands.pipelineBarrier(sourceStage,
                                 destinationStage,
                                 static_cast<vk::DependencyFlags>(0),
                                 {},
                                 {},
                                 barrier
                                 );
    });
}

vk::UniqueImageView Carrot::Image::createImageView() {
    return std::move(engine.createImageView(*vkImage, vk::Format::eR8G8B8A8Srgb));
}
