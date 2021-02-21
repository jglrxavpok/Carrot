//
// Created by jglrxavpok on 29/12/2020.
//

#include <render/shaders/ShaderStages.h>
#include "RayTracer.h"
#include "render/Image.h"
#include "ASBuilder.h"

Carrot::RayTracer::RayTracer(Carrot::Engine& engine): engine(engine) {
    // init raytracing
    auto properties = engine.getPhysicalDevice().getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();
    rayTracingProperties = properties.get<vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();

    // output images
    for(size_t i = 0; i < engine.getSwapchainImageCount(); i++) {
        auto image = make_unique<Image>(engine,
                                        vk::Extent3D{engine.getSwapchainExtent().width, engine.getSwapchainExtent().height, 1},
                                        vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage,
                                        vk::Format::eR32G32B32A32Sfloat);

        image->transitionLayout(vk::Format::eR32G32B32A32Sfloat, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);

        auto view = image->createImageView(vk::Format::eR32G32B32A32Sfloat);
        lightingImages.emplace_back(move(image));
        lightingImageViews.emplace_back(move(view));
    }
}

void Carrot::RayTracer::onSwapchainRecreation() {
    updateDescriptorSets();
}

void Carrot::RayTracer::recordCommands(uint32_t frameIndex, vk::CommandBuffer& commands) {
    commands.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, *pipeline);
    commands.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, *pipelineLayout, 0, {rtDescriptorSets[frameIndex]}, {});
    // TODO: scene data
    // TODO: push constants

    // tell the GPU how the SBT is set up
    uint32_t groupSize = alignUp(rayTracingProperties.shaderGroupHandleSize, rayTracingProperties.shaderGroupBaseAlignment);
    uint32_t groupStride = groupSize;
    vk::DeviceAddress sbtAddress = engine.getLogicalDevice().getBufferAddress({.buffer = sbtBuffer->getVulkanBuffer()});

    using Stride = vk::StridedDeviceAddressRegionKHR;
    array<Stride, 4> strideAddresses {
        Stride { sbtAddress + 0u * groupSize, groupStride, groupSize*1 }, // raygen
        Stride { sbtAddress + 1u * groupSize, groupStride, groupSize*1 }, // miss
        Stride { sbtAddress + 2u * groupSize, groupStride, groupSize*1 }, // hit
        Stride { 0u, 0u, 0u }, // callable
    };

    auto extent = engine.getSwapchainExtent();
    lightingImages[frameIndex]->transitionLayoutInline(commands, vk::Format::eR32G32B32A32Sfloat, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);
    commands.traceRaysKHR(&strideAddresses[0], &strideAddresses[1], &strideAddresses[2], &strideAddresses[3], extent.width, extent.height, 1);
    lightingImages[frameIndex]->transitionLayoutInline(commands, vk::Format::eR32G32B32A32Sfloat, vk::ImageLayout::eGeneral, vk::ImageLayout::eShaderReadOnlyOptimal);
}

vector<vk::UniqueImageView>& Carrot::RayTracer::getLightingImageViews() {
    return lightingImageViews;
}

vector<const char*> Carrot::RayTracer::getRequiredDeviceExtensions() {
    return {
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_MAINTENANCE3_EXTENSION_NAME,
        VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    };
}

void Carrot::RayTracer::updateDescriptorSets() {
    std::size_t frameIndex = 0;
    for (const auto& set : rtDescriptorSets) {
        vk::DescriptorImageInfo writeImage {
                .imageView = *this->lightingImageViews[frameIndex],
                .imageLayout = vk::ImageLayout::eGeneral,
        };
        engine.getLogicalDevice().updateDescriptorSets(vk::WriteDescriptorSet {
                .dstSet = set,
                .dstBinding = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eStorageImage,
                .pImageInfo = &writeImage,
        }, {});

        frameIndex++;
    }
}

void Carrot::RayTracer::createDescriptorSets() {
    auto& device = engine.getLogicalDevice();
    std::array<vk::DescriptorPoolSize, 2> sizes = {
            vk::DescriptorPoolSize {
                .type = vk::DescriptorType::eAccelerationStructureKHR,
                .descriptorCount = 1,
            },
            vk::DescriptorPoolSize {
                .type = vk::DescriptorType::eStorageImage,
                .descriptorCount = 1,
            },
    };
    rtDescriptorPool = device.createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo {
        .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        .maxSets = engine.getSwapchainImageCount(),
        .poolSizeCount = sizes.size(),
        .pPoolSizes = sizes.data()
    }, engine.getAllocator());

    array<vk::DescriptorSetLayoutBinding, 2> bindings = {
            vk::DescriptorSetLayoutBinding {
                .binding = 0,
                .descriptorType = vk::DescriptorType::eAccelerationStructureKHR,
                .descriptorCount = engine.getSwapchainImageCount(),
                .stageFlags = vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR
            },
            vk::DescriptorSetLayoutBinding {
                    .binding = 1,
                    .descriptorType = vk::DescriptorType::eStorageImage,
                    .descriptorCount = engine.getSwapchainImageCount(),
                    .stageFlags = vk::ShaderStageFlagBits::eRaygenKHR
            },
    };

    rtDescriptorLayout = device.createDescriptorSetLayoutUnique(vk::DescriptorSetLayoutCreateInfo {
           .bindingCount = bindings.size(),
           .pBindings = bindings.data()
    }, engine.getAllocator());

    vector<vk::DescriptorSetLayout> layouts = {engine.getSwapchainImageCount(), *rtDescriptorLayout};

    rtDescriptorSets = device.allocateDescriptorSets(vk::DescriptorSetAllocateInfo {
            .descriptorPool = *rtDescriptorPool,
            .descriptorSetCount = engine.getSwapchainImageCount(),
            .pSetLayouts = layouts.data(),
    });

    // write data to descriptor sets

    std::size_t frameIndex = 0;
    for (const auto& set : rtDescriptorSets) {
        vk::WriteDescriptorSetAccelerationStructureKHR writeAS {
            .accelerationStructureCount = 1,
            .pAccelerationStructures = &engine.getASBuilder().getTopLevelAS().as->getVulkanAS()
        };

        vk::DescriptorImageInfo writeImage {
            .imageView = *this->lightingImageViews[frameIndex],
            .imageLayout = vk::ImageLayout::eGeneral,
        };

        array<vk::WriteDescriptorSet, 2> writes = {
                vk::WriteDescriptorSet {
                        .pNext = &writeAS,
                        .dstSet = set,
                        .dstBinding = 0,
                        .descriptorCount = 1,
                        .descriptorType = vk::DescriptorType::eAccelerationStructureKHR,
                },

                vk::WriteDescriptorSet {
                        .dstSet = set,
                        .dstBinding = 1,
                        .descriptorCount = 1,
                        .descriptorType = vk::DescriptorType::eStorageImage,
                        .pImageInfo = &writeImage,
                },
        };
        device.updateDescriptorSets(writes, {});

        frameIndex++;
    }
}

void Carrot::RayTracer::createPipeline() {
    auto stages = ShaderStages(engine, {
            "resources/shaders/rt/raytrace.rchit.spv",
            "resources/shaders/rt/raytrace.rmiss.spv",
            "resources/shaders/rt/raytrace.rgen.spv",
    });

    auto stageCreations = stages.createPipelineShaderStages();

    // Ray generation
    vk::RayTracingShaderGroupCreateInfoKHR rg {
        .type = vk::RayTracingShaderGroupTypeKHR::eGeneral,
        .generalShader = 2, // index into stageCreations (same as filenames)

        .closestHitShader = VK_SHADER_UNUSED_KHR,
        .anyHitShader = VK_SHADER_UNUSED_KHR,
        .intersectionShader = VK_SHADER_UNUSED_KHR,
    };
    shaderGroups.push_back(rg);

    // Miss
    vk::RayTracingShaderGroupCreateInfoKHR mg {
            .type = vk::RayTracingShaderGroupTypeKHR::eGeneral,
            .generalShader = 1, // index into stageCreations (same as filenames)

            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR,
    };
    shaderGroups.push_back(mg);

    // Hit Group 0
    vk::RayTracingShaderGroupCreateInfoKHR hg {
            .type = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup,
            .generalShader = VK_SHADER_UNUSED_KHR,
            .closestHitShader = 0, // index into stageCreations (same as filenames)
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR,
    };
    shaderGroups.push_back(hg);

    vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo;

    // TODO: push constants

    vector<vk::DescriptorSetLayout> setLayouts = {
            *rtDescriptorLayout,
            // TODO: scene data,
    };
    pipelineLayoutCreateInfo.setLayoutCount = setLayouts.size();
    pipelineLayoutCreateInfo.pSetLayouts = setLayouts.data();

    auto& device = engine.getLogicalDevice();

    pipelineLayout = device.createPipelineLayoutUnique(pipelineLayoutCreateInfo, engine.getAllocator());

    vk::RayTracingPipelineCreateInfoKHR rayPipelineInfo{};
    rayPipelineInfo.stageCount = stageCreations.size();
    rayPipelineInfo.pStages = stageCreations.data();

    rayPipelineInfo.groupCount = shaderGroups.size();
    rayPipelineInfo.pGroups = shaderGroups.data();

    rayPipelineInfo.maxPipelineRayRecursionDepth = 1;
    rayPipelineInfo.layout = *pipelineLayout;

    pipeline = device.createRayTracingPipelineKHRUnique({}, {}, rayPipelineInfo, engine.getAllocator()).value;
}

void Carrot::RayTracer::createShaderBindingTable() {
    auto groupCount = shaderGroups.size();
    uint32_t groupHandleSize = rayTracingProperties.shaderGroupHandleSize;
    uint32_t groupSizeAligned = alignUp(groupHandleSize, rayTracingProperties.shaderGroupBaseAlignment);

    uint32_t sbtSize = groupCount*groupSizeAligned;

    auto& device = engine.getLogicalDevice();

    vector<uint8_t> shaderHandleStorage(sbtSize);

    auto result = device.getRayTracingShaderGroupHandlesKHR(*pipeline, 0, groupCount, sbtSize, shaderHandleStorage.data());
    assert(result == vk::Result::eSuccess);

    sbtBuffer = make_unique<Buffer>(engine,
                                    sbtSize,
                                    vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eShaderBindingTableKHR | vk::BufferUsageFlagBits::eTransferDst,
                                    vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible
                                    );

    sbtBuffer->setDebugNames("SBT");

    auto* pData = sbtBuffer->map<uint8_t*>();
    for (uint32_t g = 0; g < groupCount; g++) {
        memcpy(pData, shaderHandleStorage.data() + g*groupHandleSize, groupHandleSize);
        pData += groupSizeAligned;
    }
    sbtBuffer->unmap();
}

constexpr uint32_t Carrot::RayTracer::alignUp(uint32_t value, uint32_t alignment) {
    return (value + (alignment-1)) & ~(alignment -1);
}