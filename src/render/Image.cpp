//
// Created by jglrxavpok on 28/11/2020.
//

#include "Image.h"

Carrot::Image::Image(Carrot::Engine& engine, vk::Extent3D extent, vk::ImageUsageFlags usage, vk::Format format, set<uint32_t> families, vk::ImageCreateFlags flags, vk::ImageType imageType): engine(engine) {
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
