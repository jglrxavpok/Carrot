//
// Created by jglrxavpok on 30/07/2024.
//

#include "VulkanHelper.h"

#include <unordered_set>
#include <core/Macros.h>

namespace Fertilizer {
    static std::vector<std::string> requiredExtensions {
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_KHR_SWAPCHAIN_EXTENSION_NAME, // added because the engine can crash due to VulkanDriver's vk dispatcher being modified (wtf but too lazy to investiguate)
    };

    std::uint32_t VulkanHelper::findMemoryType(vk::PhysicalDevice device, std::uint32_t typeFilter, vk::MemoryPropertyFlags properties) const {
        auto memProperties = device.getMemoryProperties(*dispatch);
        for(std::uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if(typeFilter & (1 << i)
               && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
               }
        }
        throw std::runtime_error("Failed to find suitable memory type.");
    }

    GPUBuffer VulkanHelper::newHostVisibleBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage) {
        return newBuffer(size, usage, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    }

    GPUBuffer VulkanHelper::newDeviceLocalBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage) {
        return newBuffer(size, usage, vk::MemoryPropertyFlagBits::eDeviceLocal);
    }

    GPUBuffer VulkanHelper::newBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags memoryFlags) {
        vk::BufferCreateInfo createInfo {
            .size = size,
            .usage = usage,
            .sharingMode = vk::SharingMode::eExclusive,
        };
        auto vkBuffer = vkDevice->createBufferUnique(createInfo, nullptr, *dispatch);
        vk::MemoryRequirements memoryRequirements = vkDevice->getBufferMemoryRequirements(*vkBuffer, *dispatch);

        vk::MemoryAllocateFlagsInfo allocateFlags {
            .flags = vk::MemoryAllocateFlagBits::eDeviceAddress,
        };
        auto mem = vkDevice->allocateMemoryUnique(vk::MemoryAllocateInfo {
            .pNext = &allocateFlags,
            .allocationSize = memoryRequirements.size,
            .memoryTypeIndex = findMemoryType(vkPhysicalDevice, memoryRequirements.memoryTypeBits, memoryFlags),
        }, nullptr, *dispatch);
        vkDevice->bindBufferMemory(*vkBuffer, *mem, 0, *dispatch);
        return GPUBuffer {
            .vkMemory = std::move(mem),
            .vkBuffer = std::move(vkBuffer),
        };
    }

    vk::Device VulkanHelper::getDevice() {
        return *vkDevice;
    }

    vk::detail::DispatchLoaderDynamic& VulkanHelper::getDispatcher() {
        return *dispatch;
    }

    VulkanHelper::VulkanHelper() {
        // Providing a function pointer resolving vkGetInstanceProcAddr, just the few functions not depending an an instance or a device are fetched
        dispatch = std::make_unique<vk::detail::DispatchLoaderDynamic>(vkGetInstanceProcAddr);
        dispatch->init(dl);

        vk::ApplicationInfo appInfo {
            .pApplicationName = "Fertilizer",
            .apiVersion = VK_API_VERSION_1_3,
        };
        vk::InstanceCreateInfo instanceCreateInfo {
            .pApplicationInfo = &appInfo
        };
        vkInstance = vk::createInstanceUnique(instanceCreateInfo, nullptr, *dispatch);

        // Providing an already created VkInstance and a function pointer resolving vkGetInstanceProcAddr, all functions are fetched
        dispatch->init(*vkInstance);

        vkPhysicalDevice = findPhysicalDevice(*dispatch);

        vk::StructureChain deviceFeatures {
            vk::PhysicalDeviceFeatures2 {
                    .features = {
                            .independentBlend = true,

                            .multiDrawIndirect = true,
                            .drawIndirectFirstInstance = true,
                            .samplerAnisotropy = true,
                            .shaderUniformBufferArrayDynamicIndexing = true,
                            .shaderSampledImageArrayDynamicIndexing = true,
                            .shaderStorageBufferArrayDynamicIndexing = true,
                            .shaderStorageImageArrayDynamicIndexing = true,
                    },
            },
            vk::PhysicalDeviceAccelerationStructureFeaturesKHR {
                    .accelerationStructure = true,
            },
            vk::PhysicalDeviceVulkan12Features {
                    .hostQueryReset =  true,
                    .bufferDeviceAddress = true,
            },
        };
        std::vector<const char*> pEnabledExtensions;
        pEnabledExtensions.resize(requiredExtensions.size());
        for (int i = 0; i < requiredExtensions.size(); ++i) {
            pEnabledExtensions[i] = requiredExtensions[i].data();
        }

        std::uint32_t computeFamilyIndex = -1;
        const auto& queueProps = vkPhysicalDevice.getQueueFamilyProperties(*dispatch);
        for(std::size_t queueIndex = 0; queueIndex < queueProps.size(); queueIndex++) {
            const auto& queue = queueProps[queueIndex];
            if(queue.queueFlags & vk::QueueFlagBits::eCompute) {
                computeFamilyIndex = queueIndex;
            }
        }
        verify(computeFamilyIndex != (std::uint32_t)-1, "Could not find a compute queue.");

        float priority = 1.0f;
        vk::DeviceQueueCreateInfo computeQueueCreateInfo {
            .queueFamilyIndex = computeFamilyIndex,
            .queueCount = 1,
            .pQueuePriorities = &priority,
        };
        vk::DeviceCreateInfo deviceCreateInfo {
            .pNext = &deviceFeatures.get<vk::PhysicalDeviceFeatures2>(),
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &computeQueueCreateInfo,

            .enabledExtensionCount = static_cast<std::uint32_t>(pEnabledExtensions.size()),
            .ppEnabledExtensionNames = pEnabledExtensions.data(),
        };
        vkDevice = vkPhysicalDevice.createDeviceUnique(deviceCreateInfo, nullptr, *dispatch);

        dispatch->init(*vkDevice);

        vkComputeQueue = vkDevice->getQueue(computeFamilyIndex, 0, *dispatch);
        vkComputeCommandPool = vkDevice->createCommandPoolUnique(vk::CommandPoolCreateInfo {
            .queueFamilyIndex = computeFamilyIndex
        }, nullptr, *dispatch);
    }

    void VulkanHelper::executeCommands(const std::function<void(vk::CommandBuffer)>& commandGenerator) {
        vk::CommandBuffer cmds = vkDevice->allocateCommandBuffers(vk::CommandBufferAllocateInfo {
            .commandPool = *vkComputeCommandPool,
            .commandBufferCount = 1,
        }, *dispatch)[0];

        cmds.begin(vk::CommandBufferBeginInfo {
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
        }, *dispatch);
        commandGenerator(cmds);
        cmds.end(*dispatch);

        vkComputeQueue.submit(vk::SubmitInfo {
            .commandBufferCount = 1,
            .pCommandBuffers = &cmds,
        }, {}, *dispatch);
        vkComputeQueue.waitIdle(*dispatch);

        vkDevice->freeCommandBuffers(*vkComputeCommandPool, cmds, *dispatch);
    }

    vk::PhysicalDevice VulkanHelper::findPhysicalDevice(vk::detail::DispatchLoaderDynamic& dispatch) {
        const std::vector<vk::PhysicalDevice> devices = vkInstance->enumeratePhysicalDevices(dispatch);

        for (const auto& physicalDevice : devices) {
            std::unordered_set<std::string> requiredExtensionsSet { requiredExtensions.begin(), requiredExtensions.end() };

            auto deviceExtensions = physicalDevice.enumerateDeviceExtensionProperties(nullptr, dispatch);
            for(const vk::ExtensionProperties& props : deviceExtensions) {
                requiredExtensionsSet.erase(std::string{ props.extensionName.data() });
            }

            if(requiredExtensionsSet.empty()) {
                return physicalDevice;
            }
        }

        throw std::runtime_error("No GPU can support this application.");
    }


    /* static */ vk::TransformMatrixKHR VulkanHelper::glmToRTTransformMatrix(const glm::mat4& mat) {
        vk::TransformMatrixKHR rtTransform;
        for (int column = 0; column < 4; ++column) {
            for (int row = 0; row < 3; ++row) {
                rtTransform.matrix[row][column] = mat[column][row];
            }
        }
        return rtTransform;
    }

} // Fertilizer