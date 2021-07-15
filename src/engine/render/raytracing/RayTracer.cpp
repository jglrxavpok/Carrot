//
// Created by jglrxavpok on 29/12/2020.
//

#include <engine/render/shaders/ShaderStages.h>
#include "RayTracer.h"
#include "engine/render/resources/Image.h"
#include "engine/render/CameraBufferObject.h"
#include "ASBuilder.h"
#include "SceneElement.h"
#include <iostream>

constexpr int maxInstances = 31;

Carrot::RayTracer::RayTracer(Carrot::VulkanRenderer& renderer): renderer(renderer) {
    // init raytracing
    auto properties = renderer.getVulkanDriver().getPhysicalDevice().getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();
    rayTracingProperties = properties.get<vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();

    generateBuffers();
}

void Carrot::RayTracer::generateBuffers() {
    sceneBuffers.resize(renderer.getSwapchainImageCount());
    for (int i = 0; i < renderer.getSwapchainImageCount(); ++i) {
        sceneBuffers[i] = make_unique<Buffer>(renderer.getVulkanDriver(),
                                              sizeof(SceneElement)*maxInstances/* TODO: proper size for scene description*/,
                                              vk::BufferUsageFlagBits::eStorageBuffer,
                                              vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible);
    }

    lightBuffer = RaycastedShadowingLightBuffer::create(16);

    lightVkBuffers.resize(renderer.getSwapchainImageCount());
    for (int i = 0; i < renderer.getSwapchainImageCount(); ++i) {
        lightVkBuffers[i] = make_unique<Buffer>(renderer.getVulkanDriver(),
                                                lightBuffer->getStructSize(),
                                                vk::BufferUsageFlagBits::eStorageBuffer,
                                                vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible);
    }
}

void Carrot::RayTracer::onSwapchainRecreation() {
    init();
    finishInit();
/*    createRTDescriptorSets();
    createSceneDescriptorSets();*/
//    updateDescriptorSets();
}

void Carrot::RayTracer::onFrame(Carrot::Render::Context renderContext) {
    // TODO: proper size
    vector<SceneElement> sceneElements(maxInstances);
    auto& topLevel = renderer.getASBuilder().getTopLevelInstances();
    size_t maxInstance = topLevel.size();
    for (int i = 0; i < maxInstances; ++i) {
        sceneElements[i].mappedIndex = i;
        if(i < maxInstance) {
            sceneElements[i].transform = glm::inverse(topLevel[i].transform);
            sceneElements[i].transformIT = glm::transpose(glm::inverse(topLevel[i].transform));
        }
    }
    sceneBuffers[renderContext.swapchainIndex]->directUpload(sceneElements.data(), sceneElements.size()*sizeof(SceneElement));

    lightVkBuffers[renderContext.swapchainIndex]->directUpload(lightBuffer.get(), lightBuffer->getStructSize());

}

void Carrot::RayTracer::recordCommands(Carrot::Render::Context renderContext, vk::CommandBuffer& commands) {
    commands.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, *pipeline);
    renderer.bindCameraSet(vk::PipelineBindPoint::eRayTracingKHR, *pipelineLayout, renderContext, commands);
    commands.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, *pipelineLayout, 0, {rtDescriptorSets[renderContext.eye][renderContext.swapchainIndex], sceneDescriptorSets[renderContext.swapchainIndex]}, {});
    // TODO: scene data
    // TODO: push constants

    // tell the GPU how the SBT is set up
    uint32_t groupSize = alignUp(rayTracingProperties.shaderGroupHandleSize, rayTracingProperties.shaderGroupBaseAlignment);
    uint32_t groupStride = groupSize;
    vk::DeviceAddress sbtAddress = renderer.getVulkanDriver().getLogicalDevice().getBufferAddress({.buffer = sbtBuffer->getVulkanBuffer()});

    using Stride = vk::StridedDeviceAddressRegionKHR;
    array<Stride, 4> strideAddresses {
        Stride { sbtAddress + 0u * groupSize, groupStride, groupSize*1 }, // raygen
        Stride { sbtAddress + 1u * groupSize, groupStride, groupSize*1 }, // miss + miss shadow
        Stride { sbtAddress + 3u * groupSize, groupStride, groupSize*1 }, // hit
        Stride { 0u, 0u, 0u }, // callable
    };

    auto extent = renderer.getVulkanDriver().getSwapchainExtent();
    if(hasStuffToDraw) {
        commands.traceRaysKHR(&strideAddresses[0], &strideAddresses[1], &strideAddresses[2], &strideAddresses[3], extent.width, extent.height, 1);
    }
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

void Carrot::RayTracer::createRTDescriptorSets() {
    auto& device = renderer.getVulkanDriver().getLogicalDevice();
    std::array<vk::DescriptorPoolSize, 2> rtSizes = {
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
            .maxSets = static_cast<uint32_t>(renderer.getSwapchainImageCount()) * (renderer.getConfiguration().runInVR ? 2 : 1),
            .poolSizeCount = rtSizes.size(),
            .pPoolSizes = rtSizes.data()
    }, renderer.getVulkanDriver().getAllocationCallbacks());

    array<vk::DescriptorSetLayoutBinding, 2> rtBindings = {
            vk::DescriptorSetLayoutBinding {
                    .binding = 0,
                    .descriptorType = vk::DescriptorType::eAccelerationStructureKHR,
                    .descriptorCount = 1,
                    .stageFlags = vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR
            },
            vk::DescriptorSetLayoutBinding {
                    .binding = 1,
                    .descriptorType = vk::DescriptorType::eStorageImage,
                    .descriptorCount = 1,
                    .stageFlags = vk::ShaderStageFlagBits::eRaygenKHR
            },
    };

    rtDescriptorLayout = device.createDescriptorSetLayoutUnique(vk::DescriptorSetLayoutCreateInfo {
            .bindingCount = rtBindings.size(),
            .pBindings = rtBindings.data()
    }, renderer.getVulkanDriver().getAllocationCallbacks());

    vector<vk::DescriptorSetLayout> rtLayouts = {renderer.getSwapchainImageCount(), *rtDescriptorLayout};

    if(renderer.getConfiguration().runInVR) {
        rtDescriptorSets[Render::Eye::LeftEye] = device.allocateDescriptorSets(vk::DescriptorSetAllocateInfo {
                .descriptorPool = *rtDescriptorPool,
                .descriptorSetCount = static_cast<uint32_t>(renderer.getSwapchainImageCount()),
                .pSetLayouts = rtLayouts.data(),
        });
        rtDescriptorSets[Render::Eye::RightEye] = device.allocateDescriptorSets(vk::DescriptorSetAllocateInfo {
                .descriptorPool = *rtDescriptorPool,
                .descriptorSetCount = static_cast<uint32_t>(renderer.getSwapchainImageCount()),
                .pSetLayouts = rtLayouts.data(),
        });
    } else {
        rtDescriptorSets[Render::Eye::NoVR] = device.allocateDescriptorSets(vk::DescriptorSetAllocateInfo {
                .descriptorPool = *rtDescriptorPool,
                .descriptorSetCount = static_cast<uint32_t>(renderer.getSwapchainImageCount()),
                .pSetLayouts = rtLayouts.data(),
        });
    }
}

void Carrot::RayTracer::createSceneDescriptorSets() {
    auto& device = renderer.getVulkanDriver().getLogicalDevice();

    std::vector<vk::DescriptorPoolSize> sceneSizes = {
            // Camera
            vk::DescriptorPoolSize {
                    .type = vk::DescriptorType::eUniformBufferDynamic,
                    .descriptorCount = 1,
            },

            // Scene elements
            vk::DescriptorPoolSize {
                    .type = vk::DescriptorType::eStorageBuffer,
                    .descriptorCount = 1,
            },

            // Vertex buffers
            vk::DescriptorPoolSize {
                    .type = vk::DescriptorType::eStorageBuffer,
                    .descriptorCount = maxInstances /* TODO: proper size */,
            },

            // Index buffers
            vk::DescriptorPoolSize {
                    .type = vk::DescriptorType::eStorageBuffer,
                    .descriptorCount = maxInstances /* TODO: proper size */,
            },

            // Light buffer
            vk::DescriptorPoolSize {
                    .type = vk::DescriptorType::eStorageBuffer,
                    .descriptorCount = 1,
            },
    };
    sceneDescriptorPool = device.createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo {
            .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            .maxSets = static_cast<uint32_t>(renderer.getSwapchainImageCount()),
            .poolSizeCount = static_cast<uint32_t>(sceneSizes.size()),
            .pPoolSizes = sceneSizes.data()
    }, renderer.getVulkanDriver().getAllocationCallbacks());

    vector<vk::DescriptorSetLayoutBinding> sceneBindings = {
            // Scene Elements
            vk::DescriptorSetLayoutBinding {
                    .binding = 1,
                    .descriptorType = vk::DescriptorType::eStorageBuffer,
                    .descriptorCount = 1,
                    .stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR
            },

            // Vertex buffers
            vk::DescriptorSetLayoutBinding {
                    .binding = 2,
                    .descriptorType = vk::DescriptorType::eStorageBuffer,
                    .descriptorCount = maxInstances, /* TODO: proper size */
                    .stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR
            },

            // Index buffers
            vk::DescriptorSetLayoutBinding {
                    .binding = 3,
                    .descriptorType = vk::DescriptorType::eStorageBuffer,
                    .descriptorCount = maxInstances, /* TODO: proper size */
                    .stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR
            },

            // Light buffer
            vk::DescriptorSetLayoutBinding {
                    .binding = 4,
                    .descriptorType = vk::DescriptorType::eStorageBuffer,
                    .descriptorCount = 1,
                    .stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR
            },
    };

    sceneDescriptorLayout = device.createDescriptorSetLayoutUnique(vk::DescriptorSetLayoutCreateInfo {
            .bindingCount = static_cast<uint32_t>(sceneBindings.size()),
            .pBindings = sceneBindings.data()
    }, renderer.getVulkanDriver().getAllocationCallbacks());

    vector<vk::DescriptorSetLayout> sceneLayouts = {renderer.getSwapchainImageCount(), *sceneDescriptorLayout};

    sceneDescriptorSets = device.allocateDescriptorSets(vk::DescriptorSetAllocateInfo {
            .descriptorPool = *sceneDescriptorPool,
            .descriptorSetCount = static_cast<uint32_t>(renderer.getSwapchainImageCount()),
            .pSetLayouts = sceneLayouts.data(),
    });

    // write data to descriptor sets
    std::size_t frameIndex = 0;
    for (const auto& set : sceneDescriptorSets) {
        vk::DescriptorBufferInfo writeScene {
                .buffer = sceneBuffers[frameIndex]->getVulkanBuffer(),
                .offset = 0,
                .range = sizeof(SceneElement)*maxInstances/* TODO: get scene description */,
        };

        vk::DescriptorBufferInfo writeLights {
                .buffer = lightVkBuffers[frameIndex]->getVulkanBuffer(),
                .offset = 0,
                .range = lightBuffer->getStructSize(),
        };

        vector<vk::WriteDescriptorSet> writes = {
                vk::WriteDescriptorSet {
                        .dstSet = set,
                        .dstBinding = 1,
                        .descriptorCount = 1,
                        .descriptorType = vk::DescriptorType::eStorageBuffer,
                        .pBufferInfo = &writeScene,
                },

                vk::WriteDescriptorSet {
                        .dstSet = set,
                        .dstBinding = 4,
                        .descriptorCount = 1,
                        .descriptorType = vk::DescriptorType::eStorageBuffer,
                        .pBufferInfo = &writeLights,
                },
        };
        device.updateDescriptorSets(writes, {});

        frameIndex++;
    }
}

void Carrot::RayTracer::createDescriptorSets() {
    createRTDescriptorSets();
    createSceneDescriptorSets();
}

void Carrot::RayTracer::createPipeline() {
    auto stages = ShaderStages(renderer.getVulkanDriver(), {
            "resources/shaders/rt/raytrace.rgen.spv",
            "resources/shaders/rt/raytrace.rmiss.spv",
            "resources/shaders/rt/raytrace.rchit.spv",
            "resources/shaders/rt/shadow.rmiss.spv",
    });

    auto stageCreations = stages.createPipelineShaderStages();

    // Ray generation
    vk::RayTracingShaderGroupCreateInfoKHR rg {
        .type = vk::RayTracingShaderGroupTypeKHR::eGeneral,
        .generalShader = 0, // index into stageCreations (same as filenames)

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

    // Miss shadow
    vk::RayTracingShaderGroupCreateInfoKHR missShadowG {
            .type = vk::RayTracingShaderGroupTypeKHR::eGeneral,
            .generalShader = 3, // index into stageCreations (same as filenames)

            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR,
    };
    shaderGroups.push_back(missShadowG);

    // Hit Group 0
    vk::RayTracingShaderGroupCreateInfoKHR hg {
            .type = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup,
            .generalShader = VK_SHADER_UNUSED_KHR,
            .closestHitShader = 2, // index into stageCreations (same as filenames)
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR,
    };
    shaderGroups.push_back(hg);

    vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo;

    // TODO: push constants

    vector<vk::DescriptorSetLayout> setLayouts = {
            *rtDescriptorLayout,
            *sceneDescriptorLayout,
            renderer.getVulkanDriver().getMainCameraDescriptorSetLayout(),
    };
    pipelineLayoutCreateInfo.setLayoutCount = setLayouts.size();
    pipelineLayoutCreateInfo.pSetLayouts = setLayouts.data();

    auto& device = renderer.getVulkanDriver().getLogicalDevice();

    pipelineLayout = device.createPipelineLayoutUnique(pipelineLayoutCreateInfo, renderer.getVulkanDriver().getAllocationCallbacks());

    vk::RayTracingPipelineCreateInfoKHR rayPipelineInfo{};
    rayPipelineInfo.stageCount = stageCreations.size();
    rayPipelineInfo.pStages = stageCreations.data();

    rayPipelineInfo.groupCount = shaderGroups.size();
    rayPipelineInfo.pGroups = shaderGroups.data();

    rayPipelineInfo.maxPipelineRayRecursionDepth = 2;
    rayPipelineInfo.layout = *pipelineLayout;

    pipeline = device.createRayTracingPipelineKHRUnique({}, {}, rayPipelineInfo, renderer.getVulkanDriver().getAllocationCallbacks()).value;
}

void Carrot::RayTracer::createShaderBindingTable() {
    auto groupCount = shaderGroups.size();
    uint32_t groupHandleSize = rayTracingProperties.shaderGroupHandleSize;
    uint32_t groupSizeAligned = alignUp(groupHandleSize, rayTracingProperties.shaderGroupBaseAlignment);

    uint32_t sbtSize = groupCount*groupSizeAligned;

    auto& device = renderer.getVulkanDriver().getLogicalDevice();

    vector<uint8_t> shaderHandleStorage(sbtSize);

    auto result = device.getRayTracingShaderGroupHandlesKHR(*pipeline, 0, groupCount, sbtSize, shaderHandleStorage.data());
    assert(result == vk::Result::eSuccess);

    sbtBuffer = make_unique<Buffer>(renderer.getVulkanDriver(),
                                    sbtSize,
                                    vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eShaderBindingTableKHR | vk::BufferUsageFlagBits::eTransferDst,
                                    vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible
                                    );

    sbtBuffer->setDebugNames("SBT");

    auto* pData = sbtBuffer->map<uint8_t>();
    for (uint32_t g = 0; g < groupCount; g++) {
        memcpy(pData + g*groupSizeAligned, &shaderHandleStorage[g*groupHandleSize], groupHandleSize);
    }
    sbtBuffer->unmap();
}

constexpr uint32_t Carrot::RayTracer::alignUp(uint32_t value, uint32_t alignment) {
    return (value + (alignment-1)) & ~(alignment -1);
}

void Carrot::RayTracer::registerBuffer(uint32_t bindingIndex, const Buffer& vertexBuffer, vk::DeviceSize start, vk::DeviceSize length, size_t& index) {
    auto& device = renderer.getVulkanDriver().getLogicalDevice();

    vector<vk::WriteDescriptorSet> writes{renderer.getSwapchainImageCount()};
    vector<vk::DescriptorBufferInfo> bufferInfo{renderer.getSwapchainImageCount()};
    for(size_t frameIndex = 0; frameIndex < renderer.getSwapchainImageCount(); frameIndex++) {
        vk::WriteDescriptorSet& write = writes[frameIndex];
        vk::DescriptorBufferInfo& buffer = bufferInfo[frameIndex];

        buffer.buffer = vertexBuffer.getVulkanBuffer();
        buffer.offset = start;
        buffer.range = length;

        write.pBufferInfo = &buffer;
        write.descriptorType = vk::DescriptorType::eStorageBuffer;
        write.descriptorCount = 1;
        write.dstSet = sceneDescriptorSets[frameIndex];
        write.dstBinding = bindingIndex;
        write.dstArrayElement = index;
    }
    index++;
    device.updateDescriptorSets(writes, {});
}

void Carrot::RayTracer::registerVertexBuffer(const Buffer& vertexBuffer, vk::DeviceSize start, vk::DeviceSize length) {
    registerBuffer(2, vertexBuffer, start, length, vertexBufferIndex);
}

void Carrot::RayTracer::registerIndexBuffer(const Buffer& indexBuffer, vk::DeviceSize start, vk::DeviceSize length) {
    registerBuffer(3, indexBuffer, start, length, indexBufferIndex);
}

void Carrot::RayTracer::finishInit() {
    if(renderer.getASBuilder().getTopLevelAS().as == nullptr)
        return;

    // we need the TLAS to write, but it is not available before a call to buildTopLevelAS()

    auto& device = renderer.getVulkanDriver().getLogicalDevice();

    // write data to descriptor sets
    for(const auto& [eye, sets] : rtDescriptorSets) {
        for(const auto& set : sets) {
            vk::WriteDescriptorSetAccelerationStructureKHR writeAS {
                    .accelerationStructureCount = 1,
                    .pAccelerationStructures = &renderer.getASBuilder().getTopLevelAS().as->getVulkanAS(),
            };

            array<vk::WriteDescriptorSet, 1> writes = {
                    vk::WriteDescriptorSet {
                            .pNext = &writeAS,
                            .dstSet = set,
                            .dstBinding = 0,
                            .descriptorCount = 1,
                            .descriptorType = vk::DescriptorType::eAccelerationStructureKHR,
                    },
            };
            device.updateDescriptorSets(writes, {});
        }
    }

    hasStuffToDraw = true;
}

Carrot::RaycastedShadowingLightBuffer& Carrot::RayTracer::getLightBuffer() {
    return *lightBuffer;
}

void Carrot::RayTracer::init() {
    createDescriptorSets();
    createPipeline();
    createShaderBindingTable();
}

void Carrot::RayTracer::onSwapchainImageCountChange(size_t newCount) {
    if(!hasStuffToDraw)
        return;
    // TODO: handle this case
    //  call generateBuffers()

    // TODO: replace with proper handling
}

void Carrot::RayTracer::onSwapchainSizeChange(int newWidth, int newHeight) {
    finishInit();
}
