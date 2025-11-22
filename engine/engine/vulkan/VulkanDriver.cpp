//
// Created by jglrxavpok on 11/03/2021.
//

#include "VulkanDriver.h"
#include "engine/constants.h"
#include "engine/console/RuntimeOption.hpp"
#include "engine/render/raytracing/RayTracer.h"
#include "engine/render/CameraBufferObject.h"
#include "engine/render/resources/Buffer.h"
#include "engine/render/resources/Texture.h"
#include "core/io/Logging.hpp"
#include "core/io/Strings.h"
#include "engine/render/TextureRepository.h"
#include "engine/utils/Macros.h"
#include <iostream>
#include <map>
#include <set>
#include <core/containers/Vector.hpp>
#include <core/io/Serialisation.h>

#include "VulkanDefines.h"
#include "engine/vulkan/CustomTracyVulkan.h"

#include "engine/Engine.h"
#include "engine/vr/VRInterface.h"

#include "engine/utils/AftermathIntegration.h"

#ifdef AFTERMATH_ENABLE
#include <GFSDK_Aftermath_GpuCrashDump.h>
#include "engine/utils/NsightAftermathHelpers.h"
#include "engine/console/RuntimeOption.hpp"
#include "core/io/Strings.h"

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
constexpr bool USE_VULKAN_VALIDATION_LAYERS = false;
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
        VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME,
        VK_EXT_ROBUSTNESS_2_EXTENSION_NAME,
        VK_EXT_MESH_SHADER_EXTENSION_NAME,
        VK_KHR_8BIT_STORAGE_EXTENSION_NAME,
        VK_KHR_CALIBRATED_TIMESTAMPS_EXTENSION_NAME,
        VK_EXT_SHADER_IMAGE_ATOMIC_INT64_EXTENSION_NAME,
        VK_KHR_COMPUTE_SHADER_DERIVATIVES_EXTENSION_NAME,
};

static std::atomic<bool> breakOnVulkanError = false;

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverityBits,
        vk::DebugUtilsMessageTypeFlagsEXT messageType,
        const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) {
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity = static_cast<VkDebugUtilsMessageSeverityFlagBitsEXT>(messageSeverityBits);
    if(messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        Carrot::Log::info("Validation layer: %s", pCallbackData->pMessage);
    }
    if(messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        if(true
        && pCallbackData->messageIdNumber != 307231540u
        && pCallbackData->messageIdNumber != 3730154501u
        && pCallbackData->messageIdNumber != 2755938772u
        && pCallbackData->messageIdNumber != 1303270965u

        && pCallbackData->messageIdNumber != 3898698738u
        && pCallbackData->messageIdNumber != 3855709244u
        ) {
            Carrot::Log::error("[Frame %lu] (%llu) Validation layer: %s", GetRenderer().getFrameCount(), pCallbackData->messageIdNumber, pCallbackData->pMessage);

            if(breakOnVulkanError) {
                Carrot::Log::flush();
                debug_break();
                breakOnVulkanError = false;
            }
        }
    }

    if(pCallbackData->messageIdNumber == 1812873262u) /* VUID-vkCmdCopyBuffer-size-00115 */ {
        debug_break();
    } else if(pCallbackData->messageIdNumber == 2188733524u) {
        debug_break();
    } else if(pCallbackData->messageIdNumber == (std::int32_t)2281691928630u) { /* VUID-vkCmdDrawIndexed-None-08114 */
       // debug_break();
    } else if(pCallbackData->messageIdNumber == (std::int32_t)2863155226467u) { /* VUID-vkCmdBindDescriptorSets-pDescriptorSets-01979 */
        debug_break();
    } else if(pCallbackData->messageIdNumber == (std::int32_t)2711343670470u) { /* VUID-VkPresentInfoKHR-pImageIndices-01430 */
       // debug_break();
    } else if(pCallbackData->messageIdNumber == (std::int32_t)2162657188360u) { /* VUID-VkImageCreateInfo-extent-00944 */
        debug_break();
    } else if(pCallbackData->messageIdNumber == 3830750225u) /* VUID-vkDestroyBuffer-buffer-00922 */ {
         /* TODO: debug with error, might be cause of device removed: */debug_break();
    } else if(pCallbackData->messageIdNumber == 2147704939u) /* VUID-vkMapMemory-size-00681 */ {
        debug_break();
    } else if(pCallbackData->messageIdNumber == 2484131348u) /* VUID-vkCmdDrawIndexedIndirect-None-02699 */{
        debug_break();
    } else if(pCallbackData->messageIdNumber == 1318213324u) /* VUID-vkAllocateMemory-maxMemoryAllocationCount-04101 */ {
        //debug_break();
    } else if(pCallbackData->messageIdNumber == 3316570872u) /* VUID-vkCmdBindDescriptorSets-pDynamicOffsets-01972 */ {
        debug_break();
    } else if(pCallbackData->messageIdNumber == 2707007331u) /* VUID-vkCmdBindDescriptorSets-pDescriptorSets-01979 */ {
        //debug_break();
    } else if(pCallbackData->messageIdNumber == 3926398030u) /* VUID-VkWriteDescriptorSet-descriptorType-00328 */ {
        debug_break();
    } else if(strstr(pCallbackData->pMessage, "Threading-MultipleThreads") != nullptr) {
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
        //debug_break();
    } else if(strstr(pCallbackData->pMessage, "VUID-vkCmdExecuteCommands-pCommandBuffers-00092") != nullptr) {
        debug_break();
    } else if(strstr(pCallbackData->pMessage, "VUID-vkCmdBindVertexBuffers-pBuffers-00627") != nullptr) {
        debug_break();
    } else if(strstr(pCallbackData->pMessage, "UNASSIGNED-CoreValidation-Shader-InterfaceTypeMismatch") != nullptr) {
        debug_break();
    } else if(strstr(pCallbackData->pMessage, "VUID-VkBufferDeviceAddressInfo-buffer-02601") != nullptr) {
        debug_break();
    } else if(strstr(pCallbackData->pMessage, "UNASSIGNED-Device address out of bounds") != nullptr) {
     //   debug_break();
    } else if(strstr(pCallbackData->pMessage, "VUID-VkSubpassDescription-colorAttachmentCount-00845") != nullptr) {
        debug_break(); // too many render targets!
    } else if(strstr(pCallbackData->pMessage, "VUID-VkFramebufferCreateInfo-pAttachments-00877") != nullptr) {
        debug_break();
    } else if(strstr(pCallbackData->pMessage, "VUID-VkWriteDescriptorSet-descriptorType-00330") != nullptr) {
         debug_break();
    }


    return VK_FALSE;
}

static Carrot::RuntimeOption showGPUMemoryUsage("GPU/Show GPU Memory", false);

Carrot::VulkanDriver::VulkanDriver(Carrot::Window& window, Configuration config, Carrot::Engine* engine, Carrot::VR::Interface* vrInterface):
    vrInterface(vrInterface),
    mainWindow(window),
    graphicsCommandPool([&]() { return createGraphicsCommandPool(); }),
    computeCommandPool([&]() { return createComputeCommandPool(); }),
    transferCommandPool([&]() { return createTransferCommandPool(); }),
    config(std::move(config)),
    engine(engine)

{
    ZoneScoped;

    VK_LOADER_TYPE dl;
    VULKAN_HPP_DEFAULT_DISPATCHER.init(dl);

    createInstance();
    setupDebugMessenger();
    mainWindow.setMainWindow(); // set it early. It needs to be done before createSwapchain, because createSwapchain ensures all external windows have the same swapchain image count that the main window
    mainWindow.createSurface(); // required to pick a physical device (needs a present queue)
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
                                                                          .maxLod = VK_LOD_CLAMP_NONE,
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
                                                                         .maxLod = VK_LOD_CLAMP_NONE,
                                                                         .unnormalizedCoordinates = false,
                                                                 }, getAllocationCallbacks());

    depthFormat = findDepthFormat();
    mainWindow.createSwapChain();
    createUniformBuffers();

    emptyDescriptorSetLayout = device->createDescriptorSetLayoutUnique(vk::DescriptorSetLayoutCreateInfo {
        .bindingCount = 0,
    });
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
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

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
    createInfo.pfnUserCallback = (VULKAN_HPP_NAMESPACE::PFN_DebugUtilsMessengerCallbackEXT)debugCallback;
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

    physicalDeviceFeatures = physicalDevice.getFeatures();
    auto properties = physicalDevice.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceVulkan12Properties>();
    const std::string deviceName = properties.get<vk::PhysicalDeviceProperties2>().properties.deviceName;
    const std::string driverName = properties.get<vk::PhysicalDeviceVulkan12Properties>().driverName;
    const std::string driverInfo = properties.get<vk::PhysicalDeviceVulkan12Properties>().driverInfo;

    Carrot::Log::info("Selected %s / %s (%s) as the GPU", deviceName.c_str(), driverName.c_str(), driverInfo.c_str());
}

int Carrot::VulkanDriver::ratePhysicalDevice(const vk::PhysicalDevice& device) {
    QueuePartition families = findQueuePartition(device);
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

    if(deviceProperties.limits.timestampPeriod == 0) {
        return 0; // does not support timestamp queries
    }

    if(!deviceProperties.limits.timestampComputeAndGraphics) {
        return 0; // does not support timestamp queries for all computes & graphics queues.
    }

    return score;
}

Carrot::QueuePartition Carrot::VulkanDriver::findQueuePartition(vk::PhysicalDevice const &device) {
    std::vector<vk::QueueFamilyProperties> queueFamilies = device.getQueueFamilyProperties();

    // not hot path, so code is written for readability
    QueuePartition partition;

    struct QueueList {
        u32 nextIndex = 0;
        u32 count = 0;

        std::optional<u32> allocateOne() {
            if (nextIndex >= count) {
                return std::optional<u32>{};
            }
            return nextIndex++;
        }
    };

    Vector<QueueList> queuesPerFamily;
    queuesPerFamily.resize(queueFamilies.size());

    // 1. sort queues based on their properties
    for(i32 index = 0; index < queueFamilies.size(); index++) {
        vk::QueueFamilyProperties& family = queueFamilies[index];
        queuesPerFamily[index].count = family.queueCount;

        if (family.queueFlags & vk::QueueFlagBits::eGraphics) {
            partition.graphicsFamily = index;
        } else if (family.queueFlags & vk::QueueFlagBits::eCompute) {
            partition.computeFamily = index; // dedicated compute queues
        } else if (family.queueFlags & vk::QueueFlagBits::eTransfer) {
            partition.transferFamily = index; // dedicated copy queues
        }
    }

    if (!partition.graphicsFamily.has_value()) {
        return {}; // stop here, no graphics queue found, unusable device for Carrot
    }

    if (engine->getSettings().singleQueue) {
        partition.presentFamily = partition.graphicsFamily;
        partition.transferFamily = partition.graphicsFamily;
        partition.computeFamily = partition.graphicsFamily;
        partition.graphicsQueueIndex = 0;
        partition.presentQueueIndex = partition.graphicsQueueIndex;
        partition.transferQueueIndex = partition.graphicsQueueIndex;
        partition.computeQueueIndex = partition.graphicsQueueIndex;
        return partition;
    }

    if (!partition.computeFamily.has_value()) { // no dedicated compute :c
        partition.computeFamily = partition.graphicsFamily;
    }
    if (!partition.transferFamily.has_value()) { // no dedicated copy :c
        partition.transferFamily = partition.computeFamily;
    }

    partition.graphicsQueueIndex = 0; // always 0 in Carrot, we ensure we always select a device with at least one graphics queue

    // allocate a queue for dedicated compute
    QueueList& queuesForCompute = queuesPerFamily[partition.computeFamily.value()];
    if (std::optional<u32> queueIndex = queuesForCompute.allocateOne()) { // attempt to allocate a queue for dedicated compute
        partition.computeQueueIndex = queueIndex.value();
    } else { // no dedicated compute queue available, fallback to graphics queue
        partition.computeFamily = partition.graphicsFamily;
        partition.computeQueueIndex = partition.graphicsQueueIndex;
    }

    // allocate a queue for dedicated transfer
    QueueList& queuesForTransfer = queuesPerFamily[partition.transferFamily.value()];
    if (std::optional<u32> queueIndex = queuesForTransfer.allocateOne()) { // attempt to allocate a queue for dedicated compute
        partition.transferQueueIndex = queueIndex.value();
    } else { // no dedicated transfer queue available, fallback to compute queue (which may in turn fallback to graphics queue)
        partition.transferFamily = partition.computeFamily;
        partition.transferQueueIndex = partition.computeQueueIndex;
    }

    // use graphics queue for presentation
    partition.presentFamily = partition.graphicsFamily;
    partition.presentQueueIndex = partition.graphicsQueueIndex;

    return partition;
}

void Carrot::VulkanDriver::fillRenderingCapabilities() {
    physicalDeviceLimits = physicalDevice.getProperties().limits;

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

    if(availableSet.contains(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME)) {
        memoryBudgetSupported = true;
    }
}

void Carrot::VulkanDriver::createLogicalDevice() {
    queuePartition = findQueuePartition(physicalDevice);

    Carrot::Vector<vk::DeviceQueueCreateInfo> queueCreateInfoStructs{};

    Carrot::Vector<float> priorities;
    queuePartition.toCreateInfo(queueCreateInfoStructs, priorities);

    vk::StructureChain deviceFeatures {
            vk::PhysicalDeviceFeatures2 {
                    .features = {
                            .independentBlend = true,
                            .geometryShader = true, // for mesh shaders?

                            .multiDrawIndirect = true,
                            .drawIndirectFirstInstance = true,
                            .samplerAnisotropy = true,
                            .shaderStorageImageWriteWithoutFormat = true,

                            .shaderUniformBufferArrayDynamicIndexing = true,
                            .shaderSampledImageArrayDynamicIndexing = true,
                            .shaderStorageBufferArrayDynamicIndexing = true,
                            .shaderStorageImageArrayDynamicIndexing = true,
                            .shaderFloat64 = true,
                            .shaderInt64 = true,
                            .shaderInt16 = true,
                    },
            },
            vk::PhysicalDeviceMeshShaderFeaturesEXT {
                .taskShader = true,
                .meshShader = true,
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
            vk::PhysicalDeviceVulkan11Features {
                    .storageBuffer16BitAccess = true,
                    .shaderDrawParameters = true,
            },
            vk::PhysicalDeviceVulkan12Features {
                .storageBuffer8BitAccess = true,
                .uniformAndStorageBuffer8BitAccess = true,
                .storagePushConstant8 = true, // TODO: not on AMD Windows?
                .shaderBufferInt64Atomics = true,
                .shaderFloat16 = true,
                .shaderInt8 = true,
                .shaderSampledImageArrayNonUniformIndexing  = true,
                .shaderStorageBufferArrayNonUniformIndexing = true,
                .descriptorBindingPartiallyBound = true,
                .runtimeDescriptorArray = true,

                .scalarBlockLayout = true,
                .hostQueryReset = true,
                .timelineSemaphore = true,
                .bufferDeviceAddress = true,

            },
            vk::PhysicalDeviceVulkan13Features {
                .shaderDemoteToHelperInvocation = true,
                .synchronization2 = true,
                .dynamicRendering = true,
            },
            vk::PhysicalDeviceRobustness2FeaturesEXT {
                    .nullDescriptor = true,
            },
            vk::PhysicalDeviceComputeShaderDerivativesFeaturesKHR {
                    .computeDerivativeGroupQuads = true,
            },
            vk::PhysicalDeviceShaderImageAtomicInt64FeaturesEXT {
                    .shaderImageInt64Atomics = true,
            }
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

    vk::PhysicalDeviceProperties properties = physicalDevice.getProperties();
    std::string_view deviceName = properties.deviceName;
    const bool isNVIDIA = deviceName.find("NVIDIA") != std::string_view::npos;

#ifdef AFTERMATH_ENABLE
    bool useAftermath = engine->getSettings().useAftermath && isNVIDIA;
    vk::DeviceDiagnosticsConfigCreateInfoNV aftermath;
    if (useAftermath) {
        initAftermath();

        deviceExtensions.push_back(VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME);
        deviceExtensions.push_back(VK_NV_DEVICE_DIAGNOSTICS_CONFIG_EXTENSION_NAME);

        aftermath = {
            .flags =
                    vk::DeviceDiagnosticsConfigFlagBitsNV::eEnableShaderDebugInfo |
                    vk::DeviceDiagnosticsConfigFlagBitsNV::eEnableResourceTracking |
                    vk::DeviceDiagnosticsConfigFlagBitsNV::eEnableAutomaticCheckpoints
        };

        aftermath.pNext = &deviceFeatures.get<vk::PhysicalDeviceFeatures2>();
    }
#endif

    vk::DeviceCreateInfo createInfo{
#ifdef AFTERMATH_ENABLE
            .pNext = useAftermath ? (void*)&aftermath : (void*)&deviceFeatures.get<vk::PhysicalDeviceFeatures2>(),
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

    std::unordered_map<Carrot::Pair<u32, u32>, std::shared_ptr<Vulkan::SynchronizedQueue>> queues;
    auto getQueue = [&](u32 familyIndex, u32 index) {
        queues[{familyIndex, index}] = std::make_shared<Vulkan::SynchronizedQueue>(device->getQueue(familyIndex, index));
    };

    getQueue(queuePartition.transferFamily.value(), queuePartition.transferQueueIndex);
    getQueue(queuePartition.presentFamily.value(), queuePartition.presentQueueIndex);
    getQueue(queuePartition.graphicsFamily.value(), queuePartition.graphicsQueueIndex);
    getQueue(queuePartition.computeFamily.value(), queuePartition.computeQueueIndex);

    pTransferQueue = queues[{queuePartition.transferFamily.value(), queuePartition.transferQueueIndex}];
    pPresentQueue = queues[{queuePartition.presentFamily.value(), queuePartition.presentQueueIndex}];
    pGraphicsQueue = queues[{queuePartition.graphicsFamily.value(), queuePartition.graphicsQueueIndex}];
    pComputeQueue = queues[{queuePartition.computeFamily.value(), queuePartition.computeQueueIndex}];

    pTransferQueue->name("Transfer queue");
    pPresentQueue->name("Present queue");
    pGraphicsQueue->name("Graphics queue");
    pComputeQueue->name("Compute queue");
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
        std::cerr << "Device " << logicalDevice.getProperties().deviceName << " is missing following extensions: " << std::endl;
        for(const auto& requiredExt : required) {
            std::cerr << '\t' << requiredExt << std::endl;
        }
    }
    return required.empty();
}

std::uint32_t Carrot::VulkanDriver::getGraphicsQueueIndex() {
    return queuePartition.graphicsQueueIndex;
}


Carrot::SwapChainSupportDetails Carrot::VulkanDriver::querySwapChainSupport(const vk::PhysicalDevice& device) {
    return mainWindow.getSwapChainSupport(device);
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
            .queueFamilyIndex = getQueuePartitioning().graphicsFamily.value(),
    };

    return getLogicalDevice().createCommandPoolUnique(poolInfo, getAllocationCallbacks());
}

vk::UniqueCommandPool Carrot::VulkanDriver::createTransferCommandPool() {
    vk::CommandPoolCreateInfo poolInfo{
            .flags = vk::CommandPoolCreateFlagBits::eTransient, // short lived buffer (single use)
            .queueFamilyIndex = getQueuePartitioning().transferFamily.value(),
    };

    return getLogicalDevice().createCommandPoolUnique(poolInfo, getAllocationCallbacks());
}

vk::UniqueCommandPool Carrot::VulkanDriver::createComputeCommandPool() {
    vk::CommandPoolCreateInfo poolInfo{
            .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer, // short lived buffer (single use)
            .queueFamilyIndex = getQueuePartitioning().computeFamily.value(),
    };

    return getLogicalDevice().createCommandPoolUnique(poolInfo, getAllocationCallbacks());
}

std::set<std::uint32_t> Carrot::VulkanDriver::createGraphicsAndTransferFamiliesSet() {
    return {
            getQueuePartitioning().graphicsFamily.value(),
            getQueuePartitioning().transferFamily.value(),
    };
}

void Carrot::VulkanDriver::createDefaultTexture() {
    defaultTexture = std::make_unique<Carrot::Render::Texture>(*this, "resources/textures/default.png");
}

Carrot::Render::Texture& Carrot::VulkanDriver::getDefaultTexture() {
    return *defaultTexture;
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
#ifdef AFTERMATH_ENABLE
    if (engine->getSettings().useAftermath) {
        shutdownAftermath();
    }
#endif

    mainWindow.destroySwapchainAndSurface();
    executeDeferredDestructionsNow();
}

void Carrot::VulkanDriver::createUniformBuffers() {
}

size_t Carrot::VulkanDriver::getSwapchainImagesCount() const {
    return mainWindow.getSwapchainImagesCount(); // assumed that all windows share the same format
}

vk::Format Carrot::VulkanDriver::getSwapchainImageFormat() const {
    return mainWindow.getSwapchainImageFormat(); // assumed that all windows share the same format
}

Carrot::Window& Carrot::VulkanDriver::getMainWindow() {
    return mainWindow;
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

bool Carrot::QueuePartition::isComplete() const {
    return graphicsFamily.has_value() && presentFamily.has_value() && transferFamily.has_value() && computeFamily.has_value();
}

void Carrot::QueuePartition::toCreateInfo(Vector<vk::DeviceQueueCreateInfo>& output, Carrot::Vector<float>& priorities) {
    verify(isComplete(), "Partition is not valid!");
    std::unordered_map<u32, u32> familyToCount; // how many queues per family?
    std::unordered_set<Carrot::Pair<u32, u32>> distinctQueues;
    distinctQueues.insert({graphicsFamily.value(), graphicsQueueIndex});
    distinctQueues.insert({presentFamily.value(), presentQueueIndex});
    distinctQueues.insert({computeFamily.value(), computeQueueIndex});
    distinctQueues.insert({transferFamily.value(), transferQueueIndex});
    for (const auto& [family, queueIndex] : distinctQueues) {
        familyToCount[family]++;
    }

    output.ensureReserve(familyToCount.size());

    u32 totalQueueCount = 0;
    for (const auto& [familyIndex, queueCount] : familyToCount) {
        totalQueueCount += queueCount;
    }
    priorities.resize(totalQueueCount);
    priorities.fill(1.0f);

    u32 createdQueueCount = 0;
    for (const auto& [familyIndex, queueCount] : familyToCount) {
        vk::DeviceQueueCreateInfo& createInfo = output.emplaceBack();
        createInfo.queueCount = queueCount;
        createInfo.queueFamilyIndex = familyIndex;
        createInfo.pQueuePriorities = &priorities[createdQueueCount];

        createdQueueCount += queueCount;
    }
}


void Carrot::VulkanDriver::onSwapchainImageCountChange(size_t newCount) {
    /*re-*/ createUniformBuffers();
}

void Carrot::VulkanDriver::onSwapchainSizeChange(Window& window, int newWidth, int newHeight) {
    // no-op
}

const vk::Extent2D& Carrot::VulkanDriver::getFinalRenderSize(Window& window) const {
    if(config.runInVR && window.isMainWindow()) {
        auto& vrSession = GetEngine().getVRSession();
        return vrSession.getEyeRenderSize();
    } else {
        return window.getFramebufferExtent();
    }
}

Carrot::Engine& Carrot::VulkanDriver::getEngine() {
    return *engine;
}

Carrot::VulkanRenderer& Carrot::VulkanDriver::getRenderer() {
    return engine->getRenderer();
}

void Carrot::VulkanDriver::executeDeferredDestructionsNow() {
    for(auto& [swapchainIndex, list] : deferredCommandBufferDestructions) {
        for(const auto& [pool, buffer] : list) {
            device->freeCommandBuffers(pool, buffer);
        }
        list.clear();
    }
    deferredImageDestructions.clear();
    deferredImageViewDestructions.clear();
    deferredBufferDestructions.clear();
    deferredMemoryDestructions.clear();
    deferredAccelerationStructureDestructions.clear();
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

const vk::PhysicalDeviceFeatures& Carrot::VulkanDriver::getPhysicalDeviceFeatures() const {
    return physicalDeviceFeatures;
}

const vk::PhysicalDeviceLimits& Carrot::VulkanDriver::getPhysicalDeviceLimits() const {
    return physicalDeviceLimits;
}

template<typename T>
void drop(T resource) {}

void Carrot::VulkanDriver::deferDestroy(const std::string& name, vk::UniqueImage&& resource) {
    if (GetEngine().isShuttingDown()) {
        drop(std::move(resource));
        return;
    }
    Async::LockGuard g { deferredDestroysLock };
    deferredImageDestructions.push_back(std::move(DeferredImageDestruction(name, std::move(resource))));
}

void Carrot::VulkanDriver::deferDestroy(const std::string& name, vk::UniqueImageView&& resource) {
    if (GetEngine().isShuttingDown()) {
        drop(std::move(resource));
        return;
    }
    Async::LockGuard g { deferredDestroysLock };
    deferredImageViewDestructions.push_back(std::move(DeferredImageViewDestruction(name, std::move(resource))));
}

void Carrot::VulkanDriver::deferDestroy(const std::string& name, vk::UniqueBuffer&& resource) {
    if (GetEngine().isShuttingDown()) {
        drop(std::move(resource));
        return;
    }
    Async::LockGuard g { deferredDestroysLock };
    deferredBufferDestructions.push_back(std::move(DeferredBufferDestruction(name, std::move(resource))));
}

void Carrot::VulkanDriver::deferDestroy(const std::string& name, Carrot::DeviceMemory&& resource) {
    if (GetEngine().isShuttingDown()) {
        drop(std::move(resource));
        return;
    }
    Async::LockGuard g { deferredDestroysLock };
    deferredMemoryDestructions.push_back(std::move(DeferredMemoryDestruction(name, std::move(resource))));
}

void Carrot::VulkanDriver::deferDestroy(const std::string& name, vk::UniqueAccelerationStructureKHR&& resource, Carrot::BufferAllocation&& backingMemory) {
    if (GetEngine().isShuttingDown()) {
        drop(std::move(resource));
        drop(std::move(backingMemory));
        return;
    }
    Async::LockGuard g { deferredDestroysLock };
    deferredAccelerationStructureDestructions.push_back(std::move(DeferredAccelerationStructureDestruction(name, AccelerationStructureAndBackingMemory {
        .as = std::move(resource),
        .backingMemory = std::move(backingMemory),
    })));
}

void Carrot::VulkanDriver::deferCommandBufferDestruction(vk::CommandPool commandPool, vk::CommandBuffer commandBuffer) {
    if (GetEngine().isShuttingDown()) {
        device->freeCommandBuffers(commandPool, commandBuffer);
        return;
    }
    Async::LockGuard g { deferredDestroysLock };
    std::uint32_t swapchainIndex = engine->getSwapchainImageIndexRightNow();
    deferredCommandBufferDestructions[swapchainIndex].emplace_back(commandPool, commandBuffer);
}

namespace ImageSorters {
    struct NameSorter {
        const int directionMultiplier = 0;

        bool operator()(const Carrot::Image* a, const Carrot::Image* b) const {
            return a->getDebugName().compare(b->getDebugName()) * directionMultiplier < 0;
        }
    };
    struct LengthSorter {
        const int directionMultiplier = 0;

        bool operator()(const Carrot::Image* a, const Carrot::Image* b) const {
            if(a->isOwned() && b->isOwned()) {
                if(a->getMemory().getSize() == b->getMemory().getSize()) {
                    return std::less<const Carrot::Image*>{}(a, b) == (directionMultiplier == 1);
                }
                return std::less<std::uint64_t>{}(a->getMemory().getSize(), b->getMemory().getSize()) == (directionMultiplier == 1);
            } else if(a->isOwned()) {
                // a must be higher
                return directionMultiplier < 0;
            } else if(b->isOwned()) {
                // b must be higher
                return directionMultiplier > 0;
            } else {
                return false;
            }
        }
    };
}

namespace BufferSorters {
    using PairType = std::pair<vk::DeviceAddress, const Carrot::Buffer*>;

    static bool sortEqualBuffers(vk::DeviceAddress a, vk::DeviceAddress b, int directionMultiplier) {
        if(a == b) {
            return false;
        }
        if(a < b) {
            return directionMultiplier > 0;
        }
        return directionMultiplier < 0;
    }

    struct NameSorter {
        const int directionMultiplier = 0;

        bool operator()(PairType a, PairType b) const {
            auto compareResult = a.second->getDebugName().compare(b.second->getDebugName());
            if(compareResult == 0) {
                return sortEqualBuffers(a.first, b.first, directionMultiplier);
            }
            return compareResult * directionMultiplier < 0;
        }
    };
    struct StartAddressSorter {
        const int directionMultiplier = 0;

        bool operator()(PairType a, PairType b) const {
            return std::less<vk::DeviceAddress>{}(a.second->getDeviceAddress(), b.second->getDeviceAddress()) == (directionMultiplier == 1);
        }
    };
    struct LengthSorter {
        const int directionMultiplier = 0;

        bool operator()(PairType a, PairType b) const {
            if(a.second->getSize() == b.second->getSize()) {
                return sortEqualBuffers(a.first, b.first, directionMultiplier);
            }
            return std::less<std::uint64_t>{}(a.second->getSize(), b.second->getSize()) == (directionMultiplier == 1);
        }
    };
}

void Carrot::VulkanDriver::startFrame(const Carrot::Render::Context& renderContext) {
    if(memoryBudgetSupported) {
        vk::StructureChain<vk::PhysicalDeviceMemoryProperties2, vk::PhysicalDeviceMemoryBudgetPropertiesEXT> chain;
        physicalDevice.getMemoryProperties2(&chain.get<vk::PhysicalDeviceMemoryProperties2>());
        baseProperties = chain.get<vk::PhysicalDeviceMemoryProperties2>().memoryProperties;

        auto& memoryBudget = chain.get<vk::PhysicalDeviceMemoryBudgetPropertiesEXT>();
        for (int j = 0; j < VK_MAX_MEMORY_HEAPS; ++j) {
            gpuHeapBudgets[j] = memoryBudget.heapBudget[j];
            gpuHeapUsages[j] = memoryBudget.heapUsage[j];
        }
    } else {
        baseProperties = physicalDevice.getMemoryProperties();
    }

    if(showGPUMemoryUsage) {
        bool isOpen = true;
        if(ImGui::Begin("GPU Memory", &isOpen)) {
            if(ImGui::Button("Dump!")) {
                dumpActiveGPUResources();
            }
            std::size_t totalUsage = 0;
            if(memoryBudgetSupported) {
                for (int j = 0; j < VK_MAX_MEMORY_HEAPS; ++j) {
                    if(gpuHeapBudgets[j] > 0) {
                        ImGui::Text("Heap #%d", j);
                        ImGui::Text("Usage: %s (%f%%)", Carrot::IO::toReadableFormat( gpuHeapUsages[j]).c_str(), 100.0 * gpuHeapUsages[j] / (double)gpuHeapBudgets[j]);
                        ImGui::Text("Budget: %s", Carrot::IO::toReadableFormat(gpuHeapBudgets[j]).c_str());
                        ImGui::Separator();
                        totalUsage += gpuHeapUsages[j];
                    }
                }
            } else {
                ImGui::Text(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME " is not supported by your current GPU. Only tracked information is available.");
            }


            if(memoryBudgetSupported) {
                double trackedPercent = Carrot::DeviceMemory::TotalMemoryUsed.load() / (double)totalUsage;
                ImGui::Text("Tracked device memory: %s", Carrot::IO::toReadableFormat(Carrot::DeviceMemory::TotalMemoryUsed.load()).c_str());
                ImGui::Text("Untracked device memory: %s", Carrot::IO::toReadableFormat(totalUsage - Carrot::DeviceMemory::TotalMemoryUsed.load()).c_str());
                ImGui::Text("Untracked memory percentage: %f%%", 100.0 - 100.0 * trackedPercent);
            } else {
                ImGui::Text("Tracked device memory: %s", Carrot::IO::toReadableFormat(Carrot::DeviceMemory::TotalMemoryUsed.load()).c_str());
            }

            static bool showPerType = true;
            if(ImGui::CollapsingHeader("Tracked memory per location", &showPerType)) {
                if(ImGui::BeginTable("per location memory", 2)) {
                    ImGui::TableSetupColumn("Location");
                    ImGui::TableSetupColumn("Count");
                    ImGui::TableHeadersRow();

                    for(const auto& [type, size] : Carrot::DeviceMemory::MemoryUsedByLocation.snapshot()) {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%s", type.c_str());
                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%s", Carrot::IO::toReadableFormat(*size).c_str());
                    }

                    ImGui::EndTable();
                }
            }

            if(ImGui::CollapsingHeader("Show buffers")) {
                if(ImGui::BeginTable("all buffers", 5, ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_Reorderable)) {
                    ImGui::TableSetupColumn("Buffer name");
                    ImGui::TableSetupColumn("Range");
                    ImGui::TableSetupColumn("Start Address");
                    ImGui::TableSetupColumn("Length");
                    ImGui::TableSetupColumn("Range (decimal)");
                    ImGui::TableHeadersRow();

                    ImGuiTableSortSpecs* sorting = ImGui::TableGetSortSpecs();
                    std::vector<std::pair<vk::DeviceAddress, const Carrot::Buffer*>> sortedBuffers;

                    auto snapshot = Carrot::Buffer::BufferByStartAddress.snapshot();
                    sortedBuffers.reserve(snapshot.size());
                    for(auto& [address, buffer] : snapshot) {
                        sortedBuffers.emplace_back(address, *buffer);
                    }

                    if(sorting != nullptr && sorting->SpecsCount > 0) {
                        verify(sorting->SpecsCount == 1, "Only one column at a time is supported");
                        const int columnIndex = sorting->Specs[0].ColumnIndex;
                        const int directionMultiplier = sorting->Specs[0].SortDirection == ImGuiSortDirection_Descending ? -1 : 1;

                        switch(columnIndex) {
                            case 0:
                                std::sort(sortedBuffers.begin(),
                                          sortedBuffers.end(),
                                          BufferSorters::NameSorter{directionMultiplier});
                                break;

                            case 1:
                            case 2:
                            case 4:
                                std::sort(sortedBuffers.begin(),
                                          sortedBuffers.end(),
                                          BufferSorters::StartAddressSorter{directionMultiplier});
                                break;

                            case 3:
                                std::sort(sortedBuffers.begin(),
                                          sortedBuffers.end(),
                                          BufferSorters::LengthSorter{directionMultiplier});
                                break;
                        }
                    }

                    for(const auto& [address, buffer] : sortedBuffers) {
                        ImGui::TableNextRow();

                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%s", buffer->getDebugName().c_str());

                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%llx - %llx", address, address + buffer->getSize());

                        ImGui::TableSetColumnIndex(2);
                        ImGui::Text("%llx", address);

                        ImGui::TableSetColumnIndex(3);
                        ImGui::Text("%llu", buffer->getSize());

                        ImGui::TableSetColumnIndex(4);
                        ImGui::Text("%llu - %llu", address, address + buffer->getSize());
                    }

                    ImGui::EndTable();
                }
            }

            if(ImGui::CollapsingHeader("Show allocations")) {
                if(ImGui::BeginTable("all allocs", 5, ImGuiTableFlags_Resizable | /* TODO? ImGuiTableFlags_Sortable |*/ ImGuiTableFlags_Reorderable)) {
                    ImGui::TableSetupColumn("Allocation name");
                    ImGui::TableSetupColumn("Range");
                    ImGui::TableSetupColumn("Start Address");
                    ImGui::TableSetupColumn("Length");
                    ImGui::TableSetupColumn("Dedicated");
                    ImGui::TableHeadersRow();
                    using PairType = std::pair<vk::DeviceAddress, BufferAllocation::DebugData>;
                    Carrot::Vector<PairType> sortedAllocations;

                    auto snapshot = Carrot::BufferAllocation::AllocationsByStartAddress.snapshot();
                    sortedAllocations.ensureReserve(snapshot.size());
                    for(auto& [address, buffer] : snapshot) {
                        sortedAllocations.emplaceBack(address, *buffer);
                    }

                    sortedAllocations.sort([](const PairType& a, const PairType& b) {
                        return a.second.size > b.second.size;
                    });

                    for(const auto& [address, debugData] : sortedAllocations) {
                        ImGui::TableNextRow();

                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextUnformatted(debugData.name.c_str());

                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%llx - %llx", address, address + debugData.size);

                        ImGui::TableSetColumnIndex(2);
                        ImGui::Text("%llx", address);

                        ImGui::TableSetColumnIndex(3);
                        ImGui::Text("%llu", debugData.size);

                        ImGui::TableSetColumnIndex(4);
                        ImGui::TextColored(debugData.isDedicated ? ImVec4(1,0,0,1) : ImVec4(0,1,0,1), debugData.isDedicated ? "Yes" : "No");
                    }

                    ImGui::EndTable();
                }
            }

            if(ImGui::CollapsingHeader("Show images")) {
                ImGuiTextFilter filter;
                filter.Draw("Image name");

                Vector<const Carrot::Image*> sortedImages;

                Carrot::Async::LockGuard g { Carrot::Image::AliveImagesAccess };
                sortedImages.ensureReserve(Carrot::Image::AliveImages.size());
                std::uint64_t totalSize = 0;
                std::uint64_t totalFilteredSize = 0;
                for(const Carrot::Image* pImage : Carrot::Image::AliveImages) {
                    if(pImage->isOwned()) {
                        totalSize += pImage->getMemory().getSize();
                    }

                    const std::string& name = pImage->getDebugName();
                    if(!filter.PassFilter(name.c_str(), name.c_str() + name.size())) {
                        continue;
                    }

                    if(pImage->isOwned()) {
                        totalFilteredSize += pImage->getMemory().getSize();
                    }
                    sortedImages.emplaceBack(pImage);
                }

                ImGui::Text("Total size: %s", Carrot::IO::toReadableFormat(totalSize).c_str());
                ImGui::Text("Total filtered size: %s", Carrot::IO::toReadableFormat(totalFilteredSize).c_str());


                if(ImGui::BeginTable("images", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_Reorderable)) {
                    ImGui::TableSetupColumn("Image name");
                    ImGui::TableSetupColumn("Size");
                    ImGui::TableHeadersRow();

                    ImGuiTableSortSpecs* sorting = ImGui::TableGetSortSpecs();

                    if(sorting != nullptr && sorting->SpecsCount > 0) {
                        verify(sorting->SpecsCount == 1, "Only one column at a time is supported");
                        const int columnIndex = sorting->Specs[0].ColumnIndex;
                        const int directionMultiplier = sorting->Specs[0].SortDirection == ImGuiSortDirection_Descending ? -1 : 1;

                        switch(columnIndex) {
                            case 0:
                                sortedImages.sort(ImageSorters::NameSorter{});
                                break;

                            case 1:
                                sortedImages.sort(ImageSorters::LengthSorter{});
                            break;

                            default:
                                verify(false, "Invalid sort index");
                            break;
                        }
                    }

                    for(const Carrot::Image* pImage : sortedImages) {
                        ImGui::TableNextRow();

                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%s", pImage->getDebugName().c_str());

                        ImGui::TableSetColumnIndex(1);
                        if(pImage->isOwned()) {
                            ImGui::Text("%llu", pImage->getMemory().getSize());
                        } else {
                            ImGui::TextUnformatted("External image, no info");
                        }
                    }

                    ImGui::EndTable();
                }
            }
        }
        ImGui::End();

        if(!isOpen) {
            showGPUMemoryUsage.setValue(false);
        }
    }
}

void Carrot::VulkanDriver::submitGraphics(const vk::SubmitInfo2& submit, const vk::Fence& completeFence) {
    pGraphicsQueue->submit(submit, completeFence);
}

void Carrot::VulkanDriver::submitCompute(const vk::SubmitInfo2& submit, const vk::Fence& completeFence) {
    pComputeQueue->submit(submit, completeFence);
}

void Carrot::VulkanDriver::submitTransfer(const vk::SubmitInfo2& submit, const vk::Fence& completeFence) {
    pTransferQueue->submit(submit, completeFence);
}


void Carrot::VulkanDriver::submitTransfer(const vk::SubmitInfo& submit, const vk::Fence& completeFence) {
    pTransferQueue->submit(submit, completeFence);
}

void Carrot::VulkanDriver::waitGraphics() {
    GetVulkanDriver().getGraphicsQueue().waitIdle();
}

void Carrot::VulkanDriver::waitTransfer() {
    GetVulkanDriver().getTransferQueue().waitIdle();
}

void Carrot::VulkanDriver::waitCompute() {
    GetVulkanDriver().getComputeQueue().waitIdle();
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
#endif

    // Terminate on failure
    exit(1);
}

void Carrot::VulkanDriver::setMarker(vk::CommandBuffer& cmds, const std::string& markerData) {
#ifdef AFTERMATH_ENABLE
    if (engine->getSettings().useAftermath) {
        setAftermathMarker(cmds, markerData);
    }
#endif
}

bool Carrot::VulkanDriver::hasDebugNames() const {
#ifdef IS_DEBUG_BUILD
    return true;
#else
    return true;
#endif
}

void Carrot::VulkanDriver::dumpActiveGPUResources() {
    std::ofstream f { "./gpu-resources.txt" };

    f << "========== Buffers ==========" << std::endl;
    {
        Carrot::Vector<std::pair<vk::DeviceAddress, const Carrot::Buffer*>> sortedBuffers;

        auto snapshot = Carrot::Buffer::BufferByStartAddress.snapshot();
        sortedBuffers.ensureReserve(snapshot.size());
        for(auto& [address, buffer] : snapshot) {
            sortedBuffers.emplaceBack(address, *buffer);
        }

        sortedBuffers.sort(BufferSorters::LengthSorter{});

        f << "Name -- Range -- Size" << std::endl;
        for(const auto& [address, buffer] : sortedBuffers) {
            f << buffer->getDebugName();
            f << " -- ";
            f << address << " - " << address + buffer->getSize();

            f << " -- " << buffer->getSize();

            f << std::endl;
        }
    }

    f << "========== Buffer Allocations ==========\n";
    {
        using PairType = std::pair<vk::DeviceAddress, BufferAllocation::DebugData>;
        Carrot::Vector<PairType> sortedAllocations;

        auto snapshot = Carrot::BufferAllocation::AllocationsByStartAddress.snapshot();
        sortedAllocations.ensureReserve(snapshot.size());
        for(auto& [address, buffer] : snapshot) {
            sortedAllocations.emplaceBack(address, *buffer);
        }

        sortedAllocations.sort([](const PairType& a, const PairType& b) {
            return a.second.size > b.second.size;
        });

        f << "Name -- Size -- Dedicated\n";
        for(const PairType& pair : sortedAllocations) {
            f << pair.second.name;
            f << " -- ";
            f << pair.second.size;
            f << " -- ";
            f << (pair.second.isDedicated ? "true" : "false");
            f << std::endl;
        }
    }

    f << "========== Images ==========" << std::endl;
    {
        Vector<const Carrot::Image*> sortedImages;

        Carrot::Async::LockGuard g { Carrot::Image::AliveImagesAccess };
        sortedImages.ensureReserve(Carrot::Image::AliveImages.size());
        std::uint64_t totalSize = 0;
        std::uint64_t totalFilteredSize = 0;
        for(const Carrot::Image* pImage : Carrot::Image::AliveImages) {
            if(pImage->isOwned()) {
                totalSize += pImage->getMemory().getSize();
            }

            const std::string& name = pImage->getDebugName();

            if(pImage->isOwned()) {
                totalFilteredSize += pImage->getMemory().getSize();
            }
            sortedImages.emplaceBack(pImage);
        }

        sortedImages.sort(ImageSorters::LengthSorter{});

        f << "Name -- Size" << std::endl;
        for(const Carrot::Image* pImage : sortedImages) {
            f << pImage->getDebugName();
            f << " -- ";

            if(pImage->isOwned()) {
                f << pImage->getMemory().getSize();
            } else {
                f << "External image, no info";
            }
            f << std::endl;
        }
    }
}

/// Used to dump when a crash happens (can be called from debugger)
static void _DumpGPUResources() {
    GetVulkanDriver().dumpActiveGPUResources();
}
