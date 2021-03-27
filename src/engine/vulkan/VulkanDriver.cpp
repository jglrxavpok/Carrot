//
// Created by jglrxavpok on 11/03/2021.
//

#include "VulkanDriver.h"
#include "engine/constants.h"
#include "engine/render/raytracing/RayTracer.h"
#include "engine/render/DebugBufferObject.h"
#include "engine/render/CameraBufferObject.h"
#include "engine/render/resources/Buffer.h"
#include <iostream>
#include <map>
#include <set>

using namespace std;

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) {

    if(messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        std::cerr << "Validation layer: " << pCallbackData->pMessage << '\n';
    }
    if(messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
//        std::cerr << "Validation layer: " << pCallbackData->pMessage << std::endl;
    }

    return VK_FALSE;
}


Carrot::VulkanDriver::VulkanDriver(NakedPtr<GLFWwindow> window): window(window),
    graphicsCommandPool([&]() { return createGraphicsCommandPool(); }),
    computeCommandPool([&]() { return createComputeCommandPool(); }),
    transferCommandPool([&]() { return createTransferCommandPool(); })

{

    glfwGetFramebufferSize(window.get(), &framebufferWidth, &framebufferHeight);

    vk::DynamicLoader dl;
    auto vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
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

    vk::ApplicationInfo appInfo{
            .pApplicationName = WINDOW_TITLE,
            .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
            .engineVersion = VK_MAKE_VERSION(0, 1, 0),
            .apiVersion = VK_API_VERSION_1_2,
    };

    std::vector<const char*> requiredExtensions = getRequiredExtensions();

    array<vk::ValidationFeatureEnableEXT, 2> validationFeatures = {
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
        std::cout << "Required extension: " << ext << ", present = ";
        bool found = std::find_if(extensions.begin(), extensions.end(), [&](const vk::ExtensionProperties& props) {
            return strcmp(props.extensionName, ext) == 0;
        }) != extensions.end();
        std::cout << std::to_string(found) << std::endl;
    }

    instance = vk::createInstanceUnique(createInfo, allocator);

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
    createInfo.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;
    createInfo.pfnUserCallback = debugCallback;
    createInfo.pUserData = nullptr;
}

void Carrot::VulkanDriver::pickPhysicalDevice() {
    const std::vector<vk::PhysicalDevice> devices = instance->enumeratePhysicalDevices();

    std::multimap<int, vk::PhysicalDevice> candidates;
    for(const auto& physicalDevice : devices) {
        int score = ratePhysicalDevice(physicalDevice);
        candidates.insert(std::make_pair(score, physicalDevice));
    }

    if(candidates.rbegin()->first > 0) { // can best candidate run this app?
        physicalDevice = candidates.rbegin()->second;
    } else {
        throw std::runtime_error("No GPU can support this application.");
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

        if(family.queueFlags & vk::QueueFlagBits::eCompute && !(family.queueFlags & vk::QueueFlagBits::eGraphics)) {
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

#ifdef AFTERMATH_ENABLE
extern "C++" void initAftermath();
#endif

void Carrot::VulkanDriver::createLogicalDevice() {
    queueFamilies = findQueueFamilies(physicalDevice);

    float priority = 1.0f;

    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfoStructs{};
    std::set<uint32_t> uniqueQueueFamilies = { queueFamilies.presentFamily.value(), queueFamilies.graphicsFamily.value(), queueFamilies.transferFamily.value() };

    for(uint32_t queueFamily : uniqueQueueFamilies) {
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
                            .multiDrawIndirect = true,
                            .samplerAnisotropy = true,
                            .shaderUniformBufferArrayDynamicIndexing = true,
                            .shaderSampledImageArrayDynamicIndexing = true,
                            .shaderStorageBufferArrayDynamicIndexing = true,
                            .shaderStorageImageArrayDynamicIndexing = true,
                    },
            },
            vk::PhysicalDeviceDescriptorIndexingFeatures {
                    .shaderStorageBufferArrayNonUniformIndexing = true,
                    .runtimeDescriptorArray = true,
            },
            vk::PhysicalDeviceRayTracingPipelineFeaturesKHR {
                    .rayTracingPipeline = true,
            },
            vk::PhysicalDeviceRayQueryFeaturesKHR {
                    .rayQuery = true,
            },
            vk::PhysicalDeviceAccelerationStructureFeaturesKHR {
                    .accelerationStructure = true,
            },
            vk::PhysicalDeviceVulkan12Features {
                    .scalarBlockLayout = true,
                    .bufferDeviceAddress = true,
            },
    };

    vector<const char*> deviceExtensions = VULKAN_DEVICE_EXTENSIONS; // copy

    for(const auto& rayTracingExt : RayTracer::getRequiredDeviceExtensions()) {
        deviceExtensions.push_back(rayTracingExt);
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
            .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfoStructs.size()),
            .pQueueCreateInfos = queueCreateInfoStructs.data(),
            .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
            .ppEnabledExtensionNames = deviceExtensions.data(),
            .pEnabledFeatures = nullptr,
    };

    if(USE_VULKAN_VALIDATION_LAYERS) { // keep compatibility with older Vulkan implementations
        createInfo.enabledLayerCount = static_cast<uint32_t>(VULKAN_VALIDATION_LAYERS.size());
        createInfo.ppEnabledLayerNames = VULKAN_VALIDATION_LAYERS.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    device = physicalDevice.createDeviceUnique(createInfo, allocator);

    VULKAN_HPP_DEFAULT_DISPATCHER.init(*device);

    transferQueue = device->getQueue(queueFamilies.transferFamily.value(), 0);
    graphicsQueue = device->getQueue(queueFamilies.graphicsFamily.value(), 0);
    computeQueue = device->getQueue(queueFamilies.computeFamily.value(), 0);
    presentQueue = device->getQueue(queueFamilies.presentFamily.value(), 0);
}

bool Carrot::VulkanDriver::checkDeviceExtensionSupport(const vk::PhysicalDevice& logicalDevice) {
    const std::vector<vk::ExtensionProperties> available = logicalDevice.enumerateDeviceExtensionProperties(nullptr);

    std::set<std::string> required(VULKAN_DEVICE_EXTENSIONS.begin(), VULKAN_DEVICE_EXTENSIONS.end());

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
    if(glfwCreateWindowSurface(static_cast<VkInstance>(*instance), window.get(), cAllocator, &cSurface) != VK_SUCCESS) {
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

uint32_t Carrot::VulkanDriver::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) {
    auto memProperties = getPhysicalDevice().getMemoryProperties();
    for(uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if(typeFilter & (1 << i)
           && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw runtime_error("Failed to find suitable memory type.");
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

std::set<uint32_t> Carrot::VulkanDriver::createGraphicsAndTransferFamiliesSet() {
    return {
            getQueueFamilies().graphicsFamily.value(),
            getQueueFamilies().transferFamily.value(),
    };
}

void Carrot::VulkanDriver::createDefaultTexture() {
    defaultImage = Image::fromFile(*this, "resources/textures/default.png");
    defaultImageView = move(defaultImage->createImageView());
}

vk::UniqueHandle<vk::ImageView, vk::DispatchLoaderDynamic>& Carrot::VulkanDriver::getDefaultImageView() {
    return defaultImageView;
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
        glfwGetFramebufferSize(window.get(), &width, &height);

        vk::Extent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height),
        };

        actualExtent.width = max(capabilities.minImageExtent.width, min(capabilities.maxImageExtent.width, actualExtent.width));
        actualExtent.height = max(capabilities.minImageExtent.height, min(capabilities.maxImageExtent.height, actualExtent.height));

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
            .imageUsage = vk::ImageUsageFlagBits::eColorAttachment, // used for rendering

            .preTransform = swapChainSupport.capabilities.currentTransform,

            // don't try to blend with background of other windows
            .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,

            .presentMode = presentMode,
            .clipped = VK_TRUE,

            .oldSwapchain = nullptr,
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

    const auto& swapchainDeviceImages = getLogicalDevice().getSwapchainImagesKHR(*swapchain);
    swapchainImages.clear();
    for(const auto& image : swapchainDeviceImages) {
        swapchainImages.push_back(image);
    }

    this->swapchainImageFormat = surfaceFormat.format;
    this->swapchainExtent = swapchainExtent;

    depthFormat = findDepthFormat();

    createSwapChainImageViews();
}

void Carrot::VulkanDriver::createSwapChainImageViews() {
    swapchainImageViews.resize(swapchainImages.size());

    for(size_t index = 0; index < swapchainImages.size(); index++) {
        auto view = createImageView(swapchainImages[index], swapchainImageFormat);
        swapchainImageViews[index] = std::move(view);
    }
}

vk::Format Carrot::VulkanDriver::findSupportedFormat(const vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features) {
    for(auto& format : candidates) {
        vk::FormatProperties properties = getPhysicalDevice().getFormatProperties(format);

        if(tiling == vk::ImageTiling::eLinear && (properties.linearTilingFeatures & features) == features) {
            return format;
        }

        if(tiling == vk::ImageTiling::eOptimal && (properties.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    throw runtime_error("Could not find supported format");
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
}

void Carrot::VulkanDriver::cleanupSwapchain() {
    swapchainImageViews.clear();
    swapchain.reset();
}

void Carrot::VulkanDriver::createUniformBuffers() {
    vk::DeviceSize bufferSize = sizeof(Carrot::CameraBufferObject);
    cameraUniformBuffers.resize(getSwapchainImageCount(), nullptr);

    for(size_t i = 0; i < getSwapchainImageCount(); i++) {
        cameraUniformBuffers[i] = make_unique<Carrot::Buffer>(*this,
                bufferSize,
                vk::BufferUsageFlagBits::eUniformBuffer,
                vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible,
                createGraphicsAndTransferFamiliesSet());
    }

    vk::DeviceSize debugBufferSize = sizeof(Carrot::DebugBufferObject);
    debugUniformBuffers.resize(getSwapchainImageCount(), nullptr);

    for(size_t i = 0; i < getSwapchainImageCount(); i++) {
        debugUniformBuffers[i] = make_unique<Carrot::Buffer>(*this,
                                                             debugBufferSize,
                                                             vk::BufferUsageFlagBits::eUniformBuffer,
                                                             vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible,
                                                             createGraphicsAndTransferFamiliesSet());
    }
}

void Carrot::VulkanDriver::updateViewportAndScissor(vk::CommandBuffer& commands) {
    commands.setViewport(0, vk::Viewport{
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>(framebufferWidth),
            .height = static_cast<float>(framebufferHeight),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
    });

    commands.setScissor(0, vk::Rect2D {
            .offset = {0,0},
            .extent = {
                    static_cast<uint32_t>(framebufferWidth),
                    static_cast<uint32_t>(framebufferHeight),
            },
    });
}

void Carrot::VulkanDriver::recreateSwapchain() {
    glfwGetFramebufferSize(window.get(), &framebufferWidth, &framebufferHeight);
    while(framebufferWidth == 0 || framebufferHeight == 0) {
        glfwGetFramebufferSize(window.get(), &framebufferWidth, &framebufferHeight);
        glfwWaitEvents();
    }
}

bool Carrot::QueueFamilies::isComplete() const {
    return graphicsFamily.has_value() && presentFamily.has_value() && transferFamily.has_value() && computeFamily.has_value();
}