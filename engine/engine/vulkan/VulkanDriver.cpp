//
// Created by jglrxavpok on 11/03/2021.
//

#include "VulkanDriver.h"
#include "engine/constants.h"
#include "engine/render/raytracing/RayTracer.h"
#include "engine/render/DebugBufferObject.h"
#include "engine/render/CameraBufferObject.h"
#include "engine/render/resources/Buffer.h"
#include "engine/render/resources/Texture.h"
#include "core/io/Logging.hpp"
#include "engine/render/TextureRepository.h"
#include "engine/utils/Macros.h"
#include "engine/render/resources/BufferView.h"
#include <iostream>
#include <map>
#include <set>
#include "engine/vulkan/CustomTracyVulkan.h"
#include "engine/Engine.h"

#include "engine/vr/VRInterface.h"

#include "engine/utils/AftermathIntegration.h"

#ifdef AFTERMATH_ENABLE
#include <GFSDK_Aftermath_GpuCrashDump.h>
#include "engine/utils/NsightAftermathHelpers.h"

#endif

const std::vector<const char*> VULKAN_VALIDATION_LAYERS = {
        "VK_LAYER_KHRONOS_validation",
#ifndef NO_DEBUG
        //"VK_LAYER_LUNARG_monitor",
#endif
};

#ifdef NO_DEBUG
#else
    #ifdef DEBUG_MARKERS
        constexpr bool USE_DEBUG_MARKERS = true;
    #else
        constexpr bool USE_DEBUG_MARKERS = false;
    #endif
#endif

#ifdef NO_DEBUG
constexpr bool USE_VULKAN_VALIDATION_LAYERS = false;
    constexpr bool USE_DEBUG_MARKERS = false;
#else
const std::vector<const char*> VULKAN_DEBUG_EXTENSIONS = {
#ifdef DEBUG_MARKERS
        VK_EXT_DEBUG_MARKER_EXTENSION_NAME,
#endif
};
#ifdef IS_DEBUG_BUILD
constexpr bool USE_VULKAN_VALIDATION_LAYERS = true;
#else
//constexpr bool USE_VULKAN_VALIDATION_LAYERS = true;
constexpr bool USE_VULKAN_VALIDATION_LAYERS = false;
#endif
#endif

const std::vector<const char*> VULKAN_DEVICE_EXTENSIONS = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
        VK_KHR_SPIRV_1_4_EXTENSION_NAME,
        VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME,
};

static std::atomic<bool> breakOnVulkanError = false;

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) {

    if(messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        Carrot::Log::info("Validation layer: %s", pCallbackData->pMessage);
    }
    if(messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        Carrot::Log::error("Validation layer: %s", pCallbackData->pMessage);
        if(breakOnVulkanError) {
            Carrot::Log::flush();
            debug_break();
            breakOnVulkanError = false;
        }
    }

    if(strstr(pCallbackData->pMessage, "Threading-MultipleThreads") != nullptr) {
        debug_break();
    } else if(strstr(pCallbackData->pMessage, "FreeMemory-memory") != nullptr) {
        debug_break();
    } else if(strstr(pCallbackData->pMessage, "VUID-vkDestroyDevice-device-00378") != nullptr) {
        //debug_break();
    } else if(strstr(pCallbackData->pMessage, "UNASSIGNED-CoreValidation-DrawState-QueryNotReset") != nullptr) {
        debug_break();
    } else if(strstr(pCallbackData->pMessage, "VUID-vkCmdDrawIndexed-None-02699") != nullptr) {
        //debug_break();
    } else if(strstr(pCallbackData->pMessage, "VUID-vkCmdDrawIndexed-None-02706") != nullptr) {
        debug_break();
    } else if(strstr(pCallbackData->pMessage, "UNASSIGNED-CoreValidation-DrawState-DescriptorSetNotBound") != nullptr) {
        debug_break();
    } else if(strstr(pCallbackData->pMessage, "UNASSIGNED-Descriptor index out of bounds") != nullptr) {
        debug_break();
    } else if(strstr(pCallbackData->pMessage, "UNASSIGNED-CoreValidation-DrawState-InvalidCommandBuffer-VkAccelerationStructureKHR") != nullptr) {
        debug_break();
    } else if(strstr(pCallbackData->pMessage, "VUID-vkDestroyAccelerationStructureKHR-accelerationStructure-02442") != nullptr) {
        debug_break();
    } else if(strstr(pCallbackData->pMessage, "VUID-vkQueueSubmit-pCommandBuffers-00074") != nullptr) {
        debug_break();
    } else if(strstr(pCallbackData->pMessage, "UNASSIGNED-CoreValidation-DrawState-InvalidCommandBuffer-VkDescriptorSet") != nullptr) {
        debug_break();
    } else if(strstr(pCallbackData->pMessage, "VUID-vkCmdExecuteCommands-pCommandBuffers-00092") != nullptr) {
        debug_break();
    } else if(strstr(pCallbackData->pMessage, "VUID-vkCmdBindVertexBuffers-pBuffers-00627") != nullptr) {
        debug_break();
    }

    return VK_FALSE;
}


Carrot::VulkanDriver::VulkanDriver(Carrot::Window& window, Configuration config, Carrot::Engine* engine, Carrot::VR::Interface* vrInterface):
    vrInterface(vrInterface),
    window(window),
    graphicsCommandPool([&]() { return createGraphicsCommandPool(); }),
    computeCommandPool([&]() { return createComputeCommandPool(); }),
    transferCommandPool([&]() { return createTransferCommandPool(); }),
    config(std::move(config)),
    engine(engine)

{
    ZoneScoped;

    window.getFramebufferSize(framebufferWidth, framebufferHeight);

    vk::DynamicLoader dl;
    auto vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    fillRenderingCapabilities();
    createLogicalDevice();

    createTransferCommandPool();
    createGraphicsCommandPool();
    createComputeCommandPool();

    createDefaultTexture();

    nearestRepeatSampler = getLogicalDevice().createSamplerUnique({
                                                                          .magFilter = vk::Filter::eNearest,
                                                                          .minFilter = vk::Filter::eNearest,
                                                                          .mipmapMode = vk::SamplerMipmapMode::eNearest,
                                                                          .addressModeU = vk::SamplerAddressMode::eRepeat,
                                                                          .addressModeV = vk::SamplerAddressMode::eRepeat,
                                                                          .addressModeW = vk::SamplerAddressMode::eRepeat,
                                                                          .anisotropyEnable = true,
                                                                          .maxAnisotropy = 16.0f,
                                                                          .unnormalizedCoordinates = false,
                                                                  }, getAllocationCallbacks());

    linearRepeatSampler = getLogicalDevice().createSamplerUnique({
                                                                         .magFilter = vk::Filter::eLinear,
                                                                         .minFilter = vk::Filter::eLinear,
                                                                         .mipmapMode = vk::SamplerMipmapMode::eLinear,
                                                                         .addressModeU = vk::SamplerAddressMode::eRepeat,
                                                                         .addressModeV = vk::SamplerAddressMode::eRepeat,
                                                                         .addressModeW = vk::SamplerAddressMode::eRepeat,
                                                                         .anisotropyEnable = true,
                                                                         .maxAnisotropy = 16.0f,
                                                                         .unnormalizedCoordinates = false,
                                                                 }, getAllocationCallbacks());

    createSwapChain();
    createUniformBuffers();

    emptyDescriptorSetLayout = device->createDescriptorSetLayoutUnique(vk::DescriptorSetLayoutCreateInfo {
        .bindingCount = 0,
    });

    textureRepository = std::make_unique<Render::TextureRepository>(*this);
}

bool Carrot::VulkanDriver::checkValidationLayerSupport() {
    const std::vector<vk::LayerProperties> layers = vk::enumerateInstanceLayerProperties();

    for(const char* layer : VULKAN_VALIDATION_LAYERS) {
        bool found = std::find_if(layers.begin(), layers.end(), [&](const vk::LayerProperties& props) {
            return strcmp(props.layerName, layer) == 0;
        }) != layers.end();
        if(!found) {
            std::cerr << "Layer " << layer << " was not found in supported layer list." << std::endl;
            return false;
        }
    }
    return true;
}

void Carrot::VulkanDriver::createInstance() {
    if(USE_VULKAN_VALIDATION_LAYERS && !checkValidationLayerSupport()) {
        throw std::runtime_error("Could not find validation layer.");
    }

    vk::ApplicationInfo appInfo {
            .pApplicationName = config.applicationName.c_str(),
            .applicationVersion = config.applicationVersion,
            .pEngineName = config.engineName,
            .engineVersion  = config.engineVersion,
            .apiVersion = config.vulkanApiVersion,
    };

    std::vector<const char*> requiredExtensions = getRequiredExtensions();

    std::array<vk::ValidationFeatureEnableEXT, 2> validationFeatures = {
            vk::ValidationFeatureEnableEXT::eGpuAssisted,
            vk::ValidationFeatureEnableEXT::eGpuAssistedReserveBindingSlot,
    };

    vk::StructureChain<vk::InstanceCreateInfo, vk::ValidationFeaturesEXT, vk::DebugUtilsMessengerCreateInfoEXT> createChain {
            {
                    .pApplicationInfo = &appInfo,

                    .enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size()),
                    .ppEnabledExtensionNames = requiredExtensions.data(),
            },
            {
                    .enabledValidationFeatureCount = validationFeatures.size(),
                    .pEnabledValidationFeatures = validationFeatures.data(),
            },
            {}
    };
    vk::InstanceCreateInfo& createInfo = createChain.get();

    if(USE_VULKAN_VALIDATION_LAYERS) {
        createInfo.ppEnabledLayerNames = VULKAN_VALIDATION_LAYERS.data();
        createInfo.enabledLayerCount = VULKAN_VALIDATION_LAYERS.size();

        setupMessenger(createChain.get<vk::DebugUtilsMessengerCreateInfoEXT>());
    } else {
        createInfo.ppEnabledLayerNames = nullptr;
        createInfo.enabledLayerCount = 0;
        createChain.unlink<vk::DebugUtilsMessengerCreateInfoEXT>();
    }

    // check extension support before creating
    const std::vector<vk::ExtensionProperties> extensions = vk::enumerateInstanceExtensionProperties(nullptr);

    for(const auto& ext : requiredExtensions) {
        bool found = std::find_if(extensions.begin(), extensions.end(), [&](const vk::ExtensionProperties& props) {
            return strcmp(props.extensionName, ext) == 0;
        }) != extensions.end();
        Carrot::Log::info("Required extension: %s, present = %d", ext, found);
    }

    if(config.runInVR) {
        instance = vrInterface->createVulkanInstance(createInfo, allocator);
    } else {
        instance = vk::createInstanceUnique(createInfo, allocator);
    }

    VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);
}

std::vector<const char *> Carrot::VulkanDriver::getRequiredExtensions() {
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    // copy GLFW extensions
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if(USE_VULKAN_VALIDATION_LAYERS) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

void Carrot::VulkanDriver::setupDebugMessenger() {
    if(!USE_VULKAN_VALIDATION_LAYERS) return;

    vk::DebugUtilsMessengerCreateInfoEXT createInfo{};
    setupMessenger(createInfo);

    callback = instance->createDebugUtilsMessengerEXTUnique(createInfo, allocator);
}

void Carrot::VulkanDriver::setupMessenger(vk::DebugUtilsMessengerCreateInfoEXT& createInfo) {
    createInfo.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo | vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
    createInfo.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;

    //createInfo.messageType |= vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance; // OpenXR seem to do a blit with GENERAL as image layout, at least on my end
    createInfo.pfnUserCallback = debugCallback;
    createInfo.pUserData = nullptr;
}

void Carrot::VulkanDriver::pickPhysicalDevice() {
    if(config.runInVR) {
        physicalDevice = vrInterface->getVulkanPhysicalDevice();
    } else {
        const std::vector<vk::PhysicalDevice> devices = instance->enumeratePhysicalDevices();

        std::multimap<int, vk::PhysicalDevice> candidates;
        for (const auto& physicalDevice : devices) {
            int score = ratePhysicalDevice(physicalDevice);
            candidates.insert(std::make_pair(score, physicalDevice));
        }

        if (candidates.rbegin()->first > 0) { // can best candidate run this app?
            physicalDevice = candidates.rbegin()->second;
        } else {
            throw std::runtime_error("No GPU can support this application.");
        }
    }
}

int Carrot::VulkanDriver::ratePhysicalDevice(const vk::PhysicalDevice& device) {
    QueueFamilies families = findQueueFamilies(device);
    if(!families.isComplete()) // must be able to generate graphics
        return 0;

    if(!checkDeviceExtensionSupport(device))
        return 0;

    SwapChainSupportDetails swapChain = querySwapChainSupport(device);
    if(swapChain.formats.empty() || swapChain.presentModes.empty()) {
        return 0;
    }

    int score = 0;

    vk::PhysicalDeviceProperties deviceProperties = device.getProperties();

    vk::PhysicalDeviceFeatures deviceFeatures = device.getFeatures();

    // highly advantage dedicated GPUs
    if(deviceProperties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
        score += 1000;
    }

    if(!deviceFeatures.samplerAnisotropy) { // must support anisotropy
        return 0;
    }

    // prefer maximum texture size
    score += deviceProperties.limits.maxImageDimension2D;

    if(!deviceFeatures.geometryShader) { // must support geometry shaders
        return 0;
    }

    return score;
}

Carrot::QueueFamilies Carrot::VulkanDriver::findQueueFamilies(vk::PhysicalDevice const &device) {
    // TODO: check raytracing capabilities
    std::vector<vk::QueueFamilyProperties> queueFamilies = device.getQueueFamilyProperties();
    uint32_t index = 0;

    QueueFamilies families;
    for(const auto& family : queueFamilies) {
        if(family.queueFlags & vk::QueueFlagBits::eGraphics) {
            families.graphicsFamily = index;
        }

        if(family.queueFlags & vk::QueueFlagBits::eTransfer && !(family.queueFlags & vk::QueueFlagBits::eGraphics)) {
            families.transferFamily = index;
        }

        if(family.queueFlags & vk::QueueFlagBits::eCompute) {
            families.computeFamily = index;
        }

        bool presentSupport = device.getSurfaceSupportKHR(index, surface);
        if(presentSupport) {
            families.presentFamily = index;
        }

        index++;
    }

    // graphics queue implicitly support transfer & compute operations
    if(!families.transferFamily.has_value()) {
        families.transferFamily = families.graphicsFamily;
    }
    if(!families.computeFamily.has_value()) {
        families.computeFamily = families.graphicsFamily;
    }

    return families;
}

void Carrot::VulkanDriver::fillRenderingCapabilities() {
    const std::vector<vk::ExtensionProperties> available = physicalDevice.enumerateDeviceExtensionProperties(nullptr);
    std::set<std::string> availableSet;

    for(const auto& e : available) {
        availableSet.insert(e.extensionName);
    }

    // check raytracing support
    if(engine->getConfiguration().raytracingSupport != RaytracingSupport::NotSupported) {
        Carrot::Log::info("Checking raytracing support...");

        std::vector<const char*> raytracingExtensions = RayTracer::getRequiredDeviceExtensions();
        std::uint32_t rtCount = 0;
        for(const auto& ext : raytracingExtensions) {
            if(availableSet.contains(ext)) {
                rtCount++;
            }
        }

        engine->getModifiableCapabilities().supportsRaytracing = rtCount == raytracingExtensions.size();

        if(engine->getCapabilities().supportsRaytracing) {
            Carrot::Log::info("Hardware supports raytracing");
        } else {
            Carrot::Log::info("Hardware does not support raytracing");
        }
    }
}

void Carrot::VulkanDriver::createLogicalDevice() {
    queueFamilies = findQueueFamilies(physicalDevice);

    float priority = 1.0f;

    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfoStructs{};
    std::set<std::uint32_t> uniqueQueueFamilies = { queueFamilies.presentFamily.value(), queueFamilies.graphicsFamily.value(), queueFamilies.transferFamily.value() };

    for(std::uint32_t queueFamily : uniqueQueueFamilies) {
        vk::DeviceQueueCreateInfo queueCreateInfo{
                .queueFamilyIndex = queueFamily,
                .queueCount = 1,
                .pQueuePriorities = &priority,
        };

        queueCreateInfoStructs.emplace_back(queueCreateInfo);
    }

    // TODO: define features we will use

    vk::StructureChain deviceFeatures {
            vk::PhysicalDeviceFeatures2 {
                    .features = {
                            .independentBlend = true,

                            .multiDrawIndirect = true,
                            .samplerAnisotropy = true,
                            .shaderUniformBufferArrayDynamicIndexing = true,
                            .shaderSampledImageArrayDynamicIndexing = true,
                            .shaderStorageBufferArrayDynamicIndexing = true,
                            .shaderStorageImageArrayDynamicIndexing = true,
                    },
            },
            vk::PhysicalDeviceRayTracingPipelineFeaturesKHR {
                    .rayTracingPipeline = GetCapabilities().supportsRaytracing,
            },
            vk::PhysicalDeviceRayQueryFeaturesKHR {
                    .rayQuery = GetCapabilities().supportsRaytracing,
            },
            vk::PhysicalDeviceAccelerationStructureFeaturesKHR {
                    .accelerationStructure = GetCapabilities().supportsRaytracing,
            },
            vk::PhysicalDeviceSynchronization2FeaturesKHR  {
                    .synchronization2 = true,
            },
            vk::PhysicalDeviceVulkan12Features {
                    .shaderSampledImageArrayNonUniformIndexing  = true,
                    .shaderStorageBufferArrayNonUniformIndexing = true,
                    .descriptorBindingPartiallyBound = true,
                    .runtimeDescriptorArray = true,

                    .scalarBlockLayout = true,
                    .bufferDeviceAddress = true,
            },
    };

    std::vector<const char*> deviceExtensions = VULKAN_DEVICE_EXTENSIONS; // copy

    if(GetCapabilities().supportsRaytracing) {
        for(const auto& rayTracingExt : RayTracer::getRequiredDeviceExtensions()) {
            deviceExtensions.push_back(rayTracingExt);
        }
    }
#ifndef NO_DEBUG
    if(USE_DEBUG_MARKERS) {
        for (const auto& debugExt : VULKAN_DEBUG_EXTENSIONS) {
            deviceExtensions.push_back(debugExt);
        }
    }
#endif

#ifdef AFTERMATH_ENABLE
    initAftermath();

    deviceExtensions.push_back(VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME);
    deviceExtensions.push_back(VK_NV_DEVICE_DIAGNOSTICS_CONFIG_EXTENSION_NAME);

    vk::DeviceDiagnosticsConfigCreateInfoNV aftermath {
        .flags =
                vk::DeviceDiagnosticsConfigFlagBitsNV::eEnableShaderDebugInfo |
                vk::DeviceDiagnosticsConfigFlagBitsNV::eEnableResourceTracking |
                vk::DeviceDiagnosticsConfigFlagBitsNV::eEnableAutomaticCheckpoints
    };

    aftermath.pNext = &deviceFeatures.get<vk::PhysicalDeviceFeatures2>();
#endif

    vk::DeviceCreateInfo createInfo{
#ifdef AFTERMATH_ENABLE
            .pNext = &aftermath,
#else
            .pNext = &deviceFeatures.get<vk::PhysicalDeviceFeatures2>(),
#endif
            .queueCreateInfoCount = static_cast<std::uint32_t>(queueCreateInfoStructs.size()),
            .pQueueCreateInfos = queueCreateInfoStructs.data(),
            .enabledExtensionCount = static_cast<std::uint32_t>(deviceExtensions.size()),
            .ppEnabledExtensionNames = deviceExtensions.data(),
            .pEnabledFeatures = nullptr,
    };

    if(USE_VULKAN_VALIDATION_LAYERS) { // keep compatibility with older Vulkan implementations
        createInfo.enabledLayerCount = static_cast<std::uint32_t>(VULKAN_VALIDATION_LAYERS.size());
        createInfo.ppEnabledLayerNames = VULKAN_VALIDATION_LAYERS.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    if(config.runInVR) {
        device = vrInterface->createVulkanDevice(createInfo, allocator);
    } else {
        device = physicalDevice.createDeviceUnique(createInfo, allocator);
    }


    VULKAN_HPP_DEFAULT_DISPATCHER.init(*device);

    transferQueue = device->getQueue(queueFamilies.transferFamily.value(), 0);

    graphicsQueueIndex = 0;
    graphicsQueue = device->getQueue(queueFamilies.graphicsFamily.value(), graphicsQueueIndex);

    computeQueue = device->getQueue(queueFamilies.computeFamily.value(), 0);

    presentQueue = device->getQueue(queueFamilies.presentFamily.value(), 0);
}

bool Carrot::VulkanDriver::checkDeviceExtensionSupport(const vk::PhysicalDevice& logicalDevice) {
    const std::vector<vk::ExtensionProperties> available = logicalDevice.enumerateDeviceExtensionProperties(nullptr);

    std::set<std::string> required(VULKAN_DEVICE_EXTENSIONS.begin(), VULKAN_DEVICE_EXTENSIONS.end());

    if(getConfiguration().raytracingSupport == RaytracingSupport::Required) {
        for(const auto& ext : RayTracer::getRequiredDeviceExtensions()) {
            required.insert(ext);
        }
    }
#ifndef NO_DEBUG
    if(USE_DEBUG_MARKERS) {
        for(const auto& debugExtension : VULKAN_DEBUG_EXTENSIONS) {
            required.insert(debugExtension);
        }
    }
#endif

    for(const auto& ext : available) {
        required.erase(ext.extensionName);
    }

    if(!required.empty()) {
        std::cerr << "Device is missing following extensions: " << std::endl;
        for(const auto& requiredExt : required) {
            std::cerr << '\t' << requiredExt << std::endl;
        }
    }
    return required.empty();
}

void Carrot::VulkanDriver::createSurface() {
    auto cAllocator = (const VkAllocationCallbacks*) allocator;
    VkSurfaceKHR cSurface;
    if(glfwCreateWindowSurface(static_cast<VkInstance>(*instance), window.getGLFWPointer(), cAllocator, &cSurface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface.");
    }
    surface = cSurface;
}

Carrot::SwapChainSupportDetails Carrot::VulkanDriver::querySwapChainSupport(const vk::PhysicalDevice& device) {
    return {
            .capabilities = device.getSurfaceCapabilitiesKHR(surface),
            .formats = device.getSurfaceFormatsKHR(surface),
            .presentModes = device.getSurfacePresentModesKHR(surface),
    };
}

std::uint32_t Carrot::VulkanDriver::findMemoryType(std::uint32_t typeFilter, vk::MemoryPropertyFlags properties) {
    auto memProperties = getPhysicalDevice().getMemoryProperties();
    for(std::uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if(typeFilter & (1 << i)
           && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("Failed to find suitable memory type.");
}

vk::UniqueCommandPool Carrot::VulkanDriver::createGraphicsCommandPool() {
    vk::CommandPoolCreateInfo poolInfo{
            .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = getQueueFamilies().graphicsFamily.value(),
    };

    return getLogicalDevice().createCommandPoolUnique(poolInfo, getAllocationCallbacks());
}

vk::UniqueCommandPool Carrot::VulkanDriver::createTransferCommandPool() {
    vk::CommandPoolCreateInfo poolInfo{
            .flags = vk::CommandPoolCreateFlagBits::eTransient, // short lived buffer (single use)
            .queueFamilyIndex = getQueueFamilies().transferFamily.value(),
    };

    return getLogicalDevice().createCommandPoolUnique(poolInfo, getAllocationCallbacks());
}

vk::UniqueCommandPool Carrot::VulkanDriver::createComputeCommandPool() {
    vk::CommandPoolCreateInfo poolInfo{
            .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer, // short lived buffer (single use)
            .queueFamilyIndex = getQueueFamilies().computeFamily.value(),
    };

    return getLogicalDevice().createCommandPoolUnique(poolInfo, getAllocationCallbacks());
}

vk::UniqueImageView Carrot::VulkanDriver::createImageView(const vk::Image& image, vk::Format imageFormat, vk::ImageAspectFlags aspectMask, vk::ImageViewType viewType, uint32_t layerCount) {
    return getLogicalDevice().createImageViewUnique({
                                                            .image = image,
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
                                                                    .levelCount = 1,
                                                                    .baseArrayLayer = 0,
                                                                    .layerCount = layerCount,
                                                            },
                                                    }, getAllocationCallbacks());
}

std::set<std::uint32_t> Carrot::VulkanDriver::createGraphicsAndTransferFamiliesSet() {
    return {
            getQueueFamilies().graphicsFamily.value(),
            getQueueFamilies().transferFamily.value(),
    };
}

void Carrot::VulkanDriver::createDefaultTexture() {
    defaultTexture = std::make_unique<Carrot::Render::Texture>(*this, "resources/textures/default.png");
}

Carrot::Render::Texture& Carrot::VulkanDriver::getDefaultTexture() {
    return *defaultTexture;
}

vk::SurfaceFormatKHR Carrot::VulkanDriver::chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& formats) {
    for(const auto& available : formats) {
        if(available.format == vk::Format::eA8B8G8R8SrgbPack32 && available.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            return available;
        }
    }

    // TODO: rank based on format and color space

    return formats[0];
}

vk::PresentModeKHR Carrot::VulkanDriver::chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& presentModes) {
    // TODO: don't use Mailbox for VSync
    for(const auto& mode : presentModes) {
        if(mode == vk::PresentModeKHR::eMailbox) {
            return mode;
        }
    }

    // only one guaranteed
    return vk::PresentModeKHR::eFifo;
}

vk::Extent2D Carrot::VulkanDriver::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities) {
    if(capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent; // no choice
    } else {
        int width, height;
        glfwGetFramebufferSize(window.getGLFWPointer(), &width, &height);

        vk::Extent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height),
        };

        actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
        actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

        return actualExtent;
    }
}

void Carrot::VulkanDriver::createSwapChain() {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(getPhysicalDevice());

    vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    vk::PresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    vk::Extent2D swapchainExtent = chooseSwapExtent(swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount +1;
    // maxImageCount == 0 means we can request any number of image
    if(swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        // ensure we don't ask for more images than the device will be able to provide
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR createInfo{
            .surface = getSurface(),
            .minImageCount = imageCount,
            .imageFormat = surfaceFormat.format,
            .imageColorSpace = surfaceFormat.colorSpace,
            .imageExtent = swapchainExtent,
            .imageArrayLayers = 1,
            .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst, // used for rendering or blit

            .preTransform = swapChainSupport.capabilities.currentTransform,

            // don't try to blend with background of other windows
            .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,

            .presentMode = presentMode,
            .clipped = VK_TRUE,

            .oldSwapchain = *swapchain,
    };

    // image info

    uint32_t indices[] = { getQueueFamilies().graphicsFamily.value(), getQueueFamilies().presentFamily.value() };

    if(getQueueFamilies().presentFamily != getQueueFamilies().graphicsFamily) {
        // image will be shared between the 2 queues, without explicit transfers
        createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = indices;
    } else {
        // always on same queue, no need to share

        createInfo.imageSharingMode = vk::SharingMode::eExclusive;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }

    swapchain = getLogicalDevice().createSwapchainKHRUnique(createInfo, getAllocationCallbacks());

    this->swapchainImageFormat = surfaceFormat.format;
    this->windowFramebufferExtent = swapchainExtent;

    const auto& swapchainDeviceImages = getLogicalDevice().getSwapchainImagesKHR(*swapchain);
    swapchainTextures.clear();
    for(const auto& image : swapchainDeviceImages) {
        swapchainTextures.emplace_back(std::make_shared<Carrot::Render::Texture>(*this, image, vk::Extent3D {
            .width = swapchainExtent.width,
            .height = swapchainExtent.height,
            .depth = 1,
        }, swapchainImageFormat));
    }

    depthFormat = findDepthFormat();
}

vk::Format Carrot::VulkanDriver::findSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features) {
    for(auto& format : candidates) {
        vk::FormatProperties properties = getPhysicalDevice().getFormatProperties(format);

        if(tiling == vk::ImageTiling::eLinear && (properties.linearTilingFeatures & features) == features) {
            return format;
        }

        if(tiling == vk::ImageTiling::eOptimal && (properties.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    throw std::runtime_error("Could not find supported format");
}

const vk::Sampler& Carrot::VulkanDriver::getLinearSampler() const {
    return *linearRepeatSampler;
}

const vk::Sampler& Carrot::VulkanDriver::getNearestSampler() const {
    return *nearestRepeatSampler;
}

vk::Format Carrot::VulkanDriver::findDepthFormat() {
    return findSupportedFormat(
            {vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
            vk::ImageTiling::eOptimal,
            vk::FormatFeatureFlagBits::eDepthStencilAttachment);
}

Carrot::VulkanDriver::~VulkanDriver() {
    swapchain.reset();
    instance->destroySurfaceKHR(getSurface(), getAllocationCallbacks());
    // TODO: don't destroy device if in VR
}

void Carrot::VulkanDriver::cleanupSwapchain() {
    swapchainTextures.clear();
    swapchain.reset();
}

void Carrot::VulkanDriver::createUniformBuffers() {
    vk::DeviceSize debugBufferSize = sizeof(Carrot::DebugBufferObject);
    debugUniformBuffers.resize(getSwapchainImageCount(), nullptr);

    for(size_t i = 0; i < getSwapchainImageCount(); i++) {
        debugUniformBuffers[i] = std::make_unique<Carrot::Buffer>(*this,
                                                             debugBufferSize,
                                                             vk::BufferUsageFlagBits::eUniformBuffer,
                                                             vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible,
                                                             createGraphicsAndTransferFamiliesSet());
    }
}

void Carrot::VulkanDriver::updateViewportAndScissor(vk::CommandBuffer& commands, const vk::Extent2D& size) {
    commands.setViewport(0, vk::Viewport{
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>(size.width),
            .height = static_cast<float>(size.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
    });

    commands.setScissor(0, vk::Rect2D {
            .offset = {0,0},
            .extent = size,
    });
}

void Carrot::VulkanDriver::fetchNewFramebufferSize() {
    window.getFramebufferSize(framebufferWidth, framebufferHeight);
    while(framebufferWidth == 0 || framebufferHeight == 0) {
        window.getFramebufferSize(framebufferWidth, framebufferHeight);
        glfwWaitEvents();
    }
}

bool Carrot::QueueFamilies::isComplete() const {
    return graphicsFamily.has_value() && presentFamily.has_value() && transferFamily.has_value() && computeFamily.has_value();
}

void Carrot::VulkanDriver::onSwapchainImageCountChange(size_t newCount) {
    /*re-*/ createUniformBuffers();
}

void Carrot::VulkanDriver::onSwapchainSizeChange(int newWidth, int newHeight) {

}

const vk::Extent2D& Carrot::VulkanDriver::getFinalRenderSize() const {
    if(config.runInVR) {
        auto& vrSession = GetEngine().getVRSession();
        return vrSession.getEyeRenderSize();
    } else {
        return getWindowFramebufferExtent();
    }
}

Carrot::Engine& Carrot::VulkanDriver::getEngine() {
    return *engine;
}

Carrot::VulkanRenderer& Carrot::VulkanDriver::getRenderer() {
    return engine->getRenderer();
}

void Carrot::VulkanDriver::newFrame(const Carrot::Render::Context& renderContext) {
    ZoneScoped;
    std::uint32_t swapchainIndex = engine->getSwapchainImageIndexRightNow();
    for(const auto& [pool, buffer] : deferredCommandBufferDestructions[swapchainIndex]) {
        device->freeCommandBuffers(pool, buffer);
    }
    deferredCommandBufferDestructions[swapchainIndex].clear();

    {
        ZoneScopedN("Deferred destruction of resources");
        for(auto& resource : deferredImageDestructions) {
            resource.tickDown();
        }
        for(auto& resource : deferredImageViewDestructions) {
            resource.tickDown();
        }
        for(auto& resource : deferredBufferDestructions) {
            resource.tickDown();
        }
        for(auto& resource : deferredMemoryDestructions) {
            resource.tickDown();
        }
        for(auto& resource : deferredAccelerationStructureDestructions) {
            resource.tickDown();
        }

        std::erase_if(deferredImageViewDestructions, [](auto& d) {
            return d.isReadyForDestruction();
        });
        std::erase_if(deferredImageDestructions, [](auto& d) {
            return d.isReadyForDestruction();
        });
        std::erase_if(deferredBufferDestructions, [](auto& d) {
            return d.isReadyForDestruction();
        });
        std::erase_if(deferredMemoryDestructions, [](auto& d) {
            return d.isReadyForDestruction();
        });
        std::erase_if(deferredAccelerationStructureDestructions, [](auto& d) {
            return d.isReadyForDestruction();
        });
    }
}

void Carrot::VulkanDriver::deferDestroy(vk::UniqueImage&& resource) {
    deferredImageDestructions.push_back(std::move(DeferredImageDestruction(std::move(resource))));
}

void Carrot::VulkanDriver::deferDestroy(vk::UniqueImageView&& resource) {
    deferredImageViewDestructions.push_back(std::move(DeferredImageViewDestruction(std::move(resource))));
}

void Carrot::VulkanDriver::deferDestroy(vk::UniqueBuffer&& resource) {
    deferredBufferDestructions.push_back(std::move(DeferredBufferDestruction(std::move(resource))));
}

void Carrot::VulkanDriver::deferDestroy(Carrot::DeviceMemory&& resource) {
    deferredMemoryDestructions.push_back(std::move(DeferredMemoryDestruction(std::move(resource))));
}

void Carrot::VulkanDriver::deferDestroy(vk::UniqueAccelerationStructureKHR&& resource) {
    deferredAccelerationStructureDestructions.push_back(std::move(DeferredAccelerationStructureDestruction(std::move(resource))));
}

void Carrot::VulkanDriver::deferCommandBufferDestruction(vk::CommandPool commandPool, vk::CommandBuffer commandBuffer) {
    std::uint32_t swapchainIndex = engine->getSwapchainImageIndexRightNow();
    deferredCommandBufferDestructions[swapchainIndex].emplace_back(commandPool, commandBuffer);
}

void Carrot::VulkanDriver::submitGraphics(const vk::SubmitInfo& submit, const vk::Fence& completeFence) {
    graphicsQueue.submit(submit, completeFence);
}

void Carrot::VulkanDriver::submitCompute(const vk::SubmitInfo& submit, const vk::Fence& completeFence) {
    computeQueue.submit(submit, completeFence);
}

void Carrot::VulkanDriver::submitTransfer(const vk::SubmitInfo& submit, const vk::Fence& completeFence) {
    transferQueue.submit(submit, completeFence);
}

void Carrot::VulkanDriver::waitGraphics() {
    GetVulkanDriver().getGraphicsQueue().waitIdle();
}

void Carrot::VulkanDriver::waitTransfer() {
    GetVulkanDriver().getTransferQueue().waitIdle();
}

void Carrot::VulkanDriver::waitDeviceIdle() {
    std::lock_guard l { deviceMutex };
    try {
        getLogicalDevice().waitIdle();
    } catch(vk::DeviceLostError& e) {
        onDeviceLost();
    }
}

std::mutex& Carrot::VulkanDriver::getDeviceMutex() {
    return deviceMutex;
}

void Carrot::VulkanDriver::breakOnNextVulkanError() {
    breakOnVulkanError = true;
}

void Carrot::VulkanDriver::onDeviceLost() {
    Carrot::Log::error(">>> GPU has crashed!! <<<");
    Carrot::Log::flush();
#ifdef AFTERMATH_ENABLE
    Carrot::Log::error("Aftermath is enabled, a .nv-gpudump file will be created inside the working directory.");
    Carrot::Log::flush();
    /*
 * Copyright (c) 2015-2019 The Khronos Group Inc.
 * Copyright (c) 2015-2019 Valve Corporation
 * Copyright (c) 2015-2019 LunarG, Inc.
 * Copyright (c) 2019-2020 NVIDIA Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Author: Jeremy Hayes <jeremy@lunarg.com>
 */

    // https://github.com/NVIDIA/nsight-aftermath-samples/blob/6f985096067e4efdb0b0d0b78f2e64dacd591798/VkHelloNsightAftermath/VkHelloNsightAftermath.cpp

    // Device lost notification is asynchronous to the NVIDIA display
    // driver's GPU crash handling. Give the Nsight Aftermath GPU crash dump
    // thread some time to do its work before terminating the process.
    auto tdrTerminationTimeout = std::chrono::seconds(20);
    auto tStart = std::chrono::steady_clock::now();
    auto tElapsed = std::chrono::milliseconds::zero();

    GFSDK_Aftermath_CrashDump_Status status = GFSDK_Aftermath_CrashDump_Status_Unknown;
    AFTERMATH_CHECK_ERROR(GFSDK_Aftermath_GetCrashDumpStatus(&status));

    while (status != GFSDK_Aftermath_CrashDump_Status_CollectingDataFailed &&
           status != GFSDK_Aftermath_CrashDump_Status_Finished &&
           tElapsed < tdrTerminationTimeout)
    {
        // Sleep 50ms and poll the status again until timeout or Aftermath finished processing the crash dump.
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        AFTERMATH_CHECK_ERROR(GFSDK_Aftermath_GetCrashDumpStatus(&status));

        auto tEnd = std::chrono::steady_clock::now();
        tElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(tEnd - tStart);
    }

    if (status != GFSDK_Aftermath_CrashDump_Status_Finished)
    {
        std::stringstream err_msg;
        err_msg << "Unexpected crash dump status: " << status;

        // modified to use Carrot's logging system
        Carrot::Log::error(err_msg.str().c_str(), "Aftermath Error");
        Carrot::Log::flush();
    }

    // Terminate on failure
    exit(1);
#endif
}

void Carrot::VulkanDriver::setMarker(vk::CommandBuffer& cmds, const std::string& markerData) {
#ifdef AFTERMATH_ENABLE
    setAftermathMarker(cmds, markerData);
#endif
}

bool Carrot::VulkanDriver::hasDebugNames() const {
    return USE_DEBUG_MARKERS;
}