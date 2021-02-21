//
// Created by jglrxavpok on 21/11/2020.
//

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>
#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <io/IO.h>
#include <render/CameraBufferObject.h>
#include <render/shaders/ShaderStages.h>
#include "Engine.h"
#include "constants.h"
#include "render/Buffer.h"
#include "render/Image.h"
#include "render/Mesh.h"
#include "render/Model.h"
#include "render/Material.h"
#include "render/Vertex.h"
#include "render/Pipeline.h"
#include "render/InstanceData.h"
#include "render/Camera.h"
#include "render/raytracing/RayTracer.h"
#include "render/raytracing/ASBuilder.h"
#include "render/GBuffer.h"
#include "game/Game.h"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) {

    //if(messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        std::cerr << "Validation layer: " << pCallbackData->pMessage << std::endl;
    //}
    if(messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    }

    return VK_FALSE;
}

Carrot::Engine::Engine(NakedPtr<GLFWwindow> window): window(window) {
    init();
}

void Carrot::Engine::init() {
    initWindow();
    initVulkan();
}

void Carrot::Engine::run() {

    size_t currentFrame = 0;

    while(running) {
        glfwPollEvents();

        if(glfwWindowShouldClose(window.get())) {
            running = false;
        }

        drawFrame(currentFrame);

        currentFrame = (currentFrame+1) % MAX_FRAMES_IN_FLIGHT;

        FrameMark;
    }

    glfwHideWindow(window.get());

    device->waitIdle();
}

static void windowResize(GLFWwindow* window, int width, int height) {
    auto app = reinterpret_cast<Carrot::Engine*>(glfwGetWindowUserPointer(window));
    app->onWindowResize();
}

static void mouseMove(GLFWwindow* window, double xpos, double ypos) {
    auto app = reinterpret_cast<Carrot::Engine*>(glfwGetWindowUserPointer(window));
    app->onMouseMove(xpos, ypos);
}

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    auto app = reinterpret_cast<Carrot::Engine*>(glfwGetWindowUserPointer(window));
    app->onKeyEvent(key, scancode, action, mods);
}

void Carrot::Engine::initWindow() {
    glfwSetWindowUserPointer(window.get(), this);
    glfwSetFramebufferSizeCallback(window.get(), windowResize);

    glfwSetCursorPosCallback(window.get(), mouseMove);
    glfwSetKeyCallback(window.get(), keyCallback);

    glfwSetInputMode(window.get(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    grabCursor = true;

    glfwGetFramebufferSize(window.get(), &framebufferWidth, &framebufferHeight);
}

void Carrot::Engine::initVulkan() {
    vk::DynamicLoader dl;
    auto vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createComputeCommandPool();
    createGraphicsCommandPool();

    allocateGraphicsCommandBuffers();
    createTracyContexts();

    createTransferCommandPool();

    createRayTracer();
    createUIResources();
    createGBuffer();
    createRenderPasses();
    createFramebuffers();
    initImgui();
    createUniformBuffers();
    createSamplers();
    gBuffer->loadResolvePipeline();

    createDefaultTexture();
    createCamera();
    initGame();

    raytracer->createDescriptorSets();
    raytracer->createPipeline();
    raytracer->createShaderBindingTable();

    for(size_t i = 0; i < getSwapchainImageCount(); i++) {
        recordSecondaryCommandBuffers(i); // g-buffer pass and rt pass
    }
    createSynchronizationObjects();
}

void Carrot::Engine::createInstance() {
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

    vk::InstanceCreateInfo createInfo{
        .pApplicationInfo = &appInfo,

        .enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size()),
        .ppEnabledExtensionNames = requiredExtensions.data(),
    };

    if(USE_VULKAN_VALIDATION_LAYERS) {
        createInfo.ppEnabledLayerNames = VULKAN_VALIDATION_LAYERS.data();
        createInfo.enabledLayerCount = VULKAN_VALIDATION_LAYERS.size();

        vk::DebugUtilsMessengerCreateInfoEXT instanceDebugMessenger{};
        setupMessenger(instanceDebugMessenger);
        createInfo.pNext = &instanceDebugMessenger;
    } else {
        createInfo.ppEnabledLayerNames = nullptr;
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
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

bool Carrot::Engine::checkValidationLayerSupport() {
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

Carrot::Engine::~Engine() {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    tracyCtx.clear();
/*    for(size_t i = 0; i < getSwapchainImageCount(); i++) {
        TracyVkDestroy(tracyCtx[i]);
    }*/
    swapchain.reset();
    instance->destroySurfaceKHR(surface, allocator);
}

std::vector<const char *> Carrot::Engine::getRequiredExtensions() {
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    // copy GLFW extensions
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if(USE_VULKAN_VALIDATION_LAYERS) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

void Carrot::Engine::createUIResources() {
    vk::DescriptorPoolSize pool_sizes[] =
            {
                    { vk::DescriptorType::eSampler, 1000 },
                    { vk::DescriptorType::eCombinedImageSampler, 1000 },
                    { vk::DescriptorType::eSampledImage, 1000 },
                    { vk::DescriptorType::eStorageImage, 1000 },
                    { vk::DescriptorType::eUniformTexelBuffer, 1000 },
                    { vk::DescriptorType::eStorageTexelBuffer, 1000 },
                    { vk::DescriptorType::eUniformBuffer, 1000 },
                    { vk::DescriptorType::eStorageBuffer, 1000 },
                    { vk::DescriptorType::eUniformBufferDynamic, 1000 },
                    { vk::DescriptorType::eStorageBufferDynamic, 1000 },
                    { vk::DescriptorType::eInputAttachment, 1000 }
            };
    vk::DescriptorPoolCreateInfo pool_info = {};
    pool_info.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
    pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;
    imguiDescriptorPool = device->createDescriptorPoolUnique(pool_info, allocator);



    uiImages.resize(getSwapchainImageCount());
    uiImageViews.resize(getSwapchainImageCount());
    for (int i = 0; i < getSwapchainImageCount(); ++i) {
        uiImages[i] = move(make_unique<Image>(*this,
                                                  vk::Extent3D{swapchainExtent.width, swapchainExtent.height, 1},
                                                  vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
                                                  vk::Format::eR8G8B8A8Unorm));

        auto view = uiImages[i]->createImageView();
        uiImageViews[i] = std::move(view);
    }
}

void Carrot::Engine::setupDebugMessenger() {
    if(!USE_VULKAN_VALIDATION_LAYERS) return;

    vk::DebugUtilsMessengerCreateInfoEXT createInfo{};
    setupMessenger(createInfo);

    callback = instance->createDebugUtilsMessengerEXTUnique(createInfo, allocator);
}

void Carrot::Engine::setupMessenger(vk::DebugUtilsMessengerCreateInfoEXT& createInfo) {
    createInfo.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo | vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
    createInfo.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;
    createInfo.pfnUserCallback = debugCallback;
    createInfo.pUserData = nullptr;
}

void Carrot::Engine::pickPhysicalDevice() {
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

int Carrot::Engine::ratePhysicalDevice(const vk::PhysicalDevice& device) {
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

Carrot::QueueFamilies Carrot::Engine::findQueueFamilies(vk::PhysicalDevice const &device) {
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

void Carrot::Engine::createLogicalDevice() {
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

    graphicsQueue = device->getQueue(queueFamilies.graphicsFamily.value(), 0);
    presentQueue = device->getQueue(queueFamilies.presentFamily.value(), 0);
    transferQueue = device->getQueue(queueFamilies.transferFamily.value(), 0);
    computeQueue = device->getQueue(queueFamilies.computeFamily.value(), 0);
}

void Carrot::Engine::createSurface() {
    auto cAllocator = (const VkAllocationCallbacks*) (allocator);
    VkSurfaceKHR cSurface;
    if(glfwCreateWindowSurface(static_cast<VkInstance>(*instance), window.get(), cAllocator, &cSurface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface.");
    }
    surface = cSurface;
}

bool Carrot::Engine::checkDeviceExtensionSupport(const vk::PhysicalDevice& logicalDevice) {
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

Carrot::SwapChainSupportDetails Carrot::Engine::querySwapChainSupport(const vk::PhysicalDevice& device) {
    return {
            .capabilities = device.getSurfaceCapabilitiesKHR(surface),
            .formats = device.getSurfaceFormatsKHR(surface),
            .presentModes = device.getSurfacePresentModesKHR(surface),
    };
}

vk::SurfaceFormatKHR Carrot::Engine::chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& formats) {
    for(const auto& available : formats) {
        if(available.format == vk::Format::eA8B8G8R8SrgbPack32 && available.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            return available;
        }
    }

    // TODO: rank based on format and color space

    return formats[0];
}

vk::PresentModeKHR Carrot::Engine::chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& presentModes) {
    for(const auto& mode : presentModes) {
        if(mode == vk::PresentModeKHR::eMailbox) {
            return mode;
        }
    }

    // only one guaranteed
    return vk::PresentModeKHR::eFifo;
}

vk::Extent2D Carrot::Engine::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities) {
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

void Carrot::Engine::createSwapChain() {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

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
        .surface = surface,
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

    QueueFamilies queueFamilies = findQueueFamilies(physicalDevice);
    uint32_t indices[] = { queueFamilies.graphicsFamily.value(), queueFamilies.presentFamily.value() };

    if(queueFamilies.presentFamily != queueFamilies.graphicsFamily) {
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

    swapchain = device->createSwapchainKHRUnique(createInfo, allocator);

    const auto& swapchainDeviceImages = device->getSwapchainImagesKHR(*swapchain);
    swapchainImages.clear();
    for(const auto& image : swapchainDeviceImages) {
        swapchainImages.push_back(image);
    }

    this->swapchainImageFormat = surfaceFormat.format;
    this->swapchainExtent = swapchainExtent;

    depthFormat = findDepthFormat();

    createSwapChainImageViews();
}

void Carrot::Engine::createSwapChainImageViews() {
    swapchainImageViews.resize(swapchainImages.size());

    for(size_t index = 0; index < swapchainImages.size(); index++) {
        auto view = Engine::createImageView(swapchainImages[index], swapchainImageFormat);
        swapchainImageViews[index] = std::move(view);
    }
}

vk::UniqueImageView Carrot::Engine::createImageView(const vk::Image& image, vk::Format imageFormat, vk::ImageAspectFlags aspectMask) const {
    return device->createImageViewUnique({
                                                 .image = image,
                                                 .viewType = vk::ImageViewType::e2D,
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
                                                         .layerCount = 1,
                                                 },
                                             }, allocator);
}

void Carrot::Engine::createRenderPasses() {
    gRenderPass = gBuffer->createRenderPass();

    vk::AttachmentDescription attachments[] = {
            {
                .format = vk::Format::eR8G8B8A8Unorm,
                .samples = vk::SampleCountFlagBits::e1,
                .loadOp = vk::AttachmentLoadOp::eClear,
                .storeOp = vk::AttachmentStoreOp::eStore,
                .initialLayout = vk::ImageLayout::eUndefined,
                .finalLayout = vk::ImageLayout::eColorAttachmentOptimal,
            }
    };

    vk::SubpassDependency dependencies[] = {
            {
                    .srcSubpass = VK_SUBPASS_EXTERNAL,
                    .dstSubpass = 0,

                    .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                                    vk::PipelineStageFlagBits::eEarlyFragmentTests,
                    // TODO: .srcAccessMask = 0,

                    .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                                    vk::PipelineStageFlagBits::eEarlyFragmentTests,
                    .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
            }
    };

    vk::AttachmentReference colorAttachment = {
            .attachment = 0,
            .layout = vk::ImageLayout::eColorAttachmentOptimal,
    };

    vk::SubpassDescription subpasses[] = {
            {
                    .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
                    .inputAttachmentCount = 0,
                    .pInputAttachments = nullptr,

                    .colorAttachmentCount = 1,
                    // index in this array is used by `layout(location = 0)` inside shaders
                    .pColorAttachments = &colorAttachment,
                    .pDepthStencilAttachment = nullptr,

                    .preserveAttachmentCount = 0,
            }
    };

    vk::RenderPassCreateInfo imguiRenderPassInfo {
        .attachmentCount = 1,
        .pAttachments = attachments,
        .subpassCount = 1,
        .pSubpasses = subpasses,
        .dependencyCount = 1,
        .pDependencies = dependencies,
    };
    imguiRenderPass = device->createRenderPassUnique(imguiRenderPassInfo);
}

void Carrot::Engine::createFramebuffers() {
    swapchainFramebuffers.resize(swapchainImages.size());
    imguiFramebuffers.resize(swapchainImages.size());

    for (size_t i = 0; i < swapchainImageViews.size(); ++i) {
        vector<vk::ImageView> attachments = {
                *swapchainImageViews[i],
        };

        gBuffer->addFramebufferAttachments(i, attachments);

        vk::FramebufferCreateInfo swapchainFramebufferInfo {
                .renderPass = *gRenderPass,
                .attachmentCount = static_cast<uint32_t>(attachments.size()),
                .pAttachments = attachments.data(),
                .width = swapchainExtent.width,
                .height = swapchainExtent.height,
                .layers = 1,
        };

        swapchainFramebuffers[i] = std::move(device->createFramebufferUnique(swapchainFramebufferInfo, allocator));

        vk::ImageView imguiAttachment[] = {
            *uiImageViews[i]
        };
        vk::FramebufferCreateInfo imguiFramebufferInfo {
                .renderPass = *imguiRenderPass,
                .attachmentCount = 1,
                .pAttachments = imguiAttachment,
                .width = swapchainExtent.width,
                .height = swapchainExtent.height,
                .layers = 1,
        };
        imguiFramebuffers[i] = std::move(device->createFramebufferUnique(imguiFramebufferInfo, allocator));
    }
}

void Carrot::Engine::createGraphicsCommandPool() {
    vk::CommandPoolCreateInfo poolInfo{
            .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = queueFamilies.graphicsFamily.value(),
    };

    graphicsCommandPool = device->createCommandPoolUnique(poolInfo, allocator);
}

void Carrot::Engine::createTransferCommandPool() {
    vk::CommandPoolCreateInfo poolInfo{
            .flags = vk::CommandPoolCreateFlagBits::eTransient, // short lived buffer (single use)
            .queueFamilyIndex = queueFamilies.transferFamily.value(),
    };

    transferCommandPool = device->createCommandPoolUnique(poolInfo, allocator);
}

void Carrot::Engine::createComputeCommandPool() {
    vk::CommandPoolCreateInfo poolInfo{
            .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer, // short lived buffer (single use)
            .queueFamilyIndex = queueFamilies.computeFamily.value(),
    };

    computeCommandPool = device->createCommandPoolUnique(poolInfo, allocator);
}

static void check_vk_result(VkResult err)
{
    if (err == 0)
        return;
    fprintf(stderr, "[vulkan-imgui] Error: VkResult = %d\n", err);
    if (err < 0)
        abort();
}

void Carrot::Engine::initImgui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForVulkan(window.get(), true);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = *instance;
    init_info.PhysicalDevice = physicalDevice;
    init_info.Device = getLogicalDevice();
    init_info.QueueFamily = getQueueFamilies().graphicsFamily.value();
    init_info.Queue = getGraphicsQueue();
    init_info.PipelineCache = nullptr;
    init_info.DescriptorPool = *imguiDescriptorPool;
    init_info.Allocator = nullptr;

    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount +1;
    // maxImageCount == 0 means we can request any number of image
    if(swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        // ensure we don't ask for more images than the device will be able to provide
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    init_info.MinImageCount = swapChainSupport.capabilities.minImageCount;
    init_info.ImageCount = imageCount;
    init_info.CheckVkResultFn = check_vk_result;
    ImGui_ImplVulkan_Init(&init_info, *imguiRenderPass);

    // Upload fonts
    performSingleTimeGraphicsCommands([&](vk::CommandBuffer& buffer) {
        ImGui_ImplVulkan_CreateFontsTexture(buffer);
        ImGui_ImplVulkan_DestroyFontUploadObjects();
    });
}

void Carrot::Engine::createRayTracer() {
    raytracer = make_unique<RayTracer>(*this);
    asBuilder = make_unique<ASBuilder>(*this);
}

void Carrot::Engine::createGBuffer() {
    gBuffer = make_unique<GBuffer>(*this, *raytracer);
}

void Carrot::Engine::recordSecondaryCommandBuffers(size_t frameIndex) {
    recordGBufferPass(frameIndex, gBufferCommandBuffers[frameIndex]);

    vk::CommandBufferInheritanceInfo gResolveInheritance {
            .renderPass = *this->gRenderPass,
            .subpass = RenderPasses::GResolveSubPassIndex,
            .framebuffer = *this->swapchainFramebuffers[frameIndex],
    };
    gBuffer->recordResolvePass(frameIndex, gResolveCommandBuffers[frameIndex], &gResolveInheritance);
}

void Carrot::Engine::recordGBufferPass(size_t frameIndex, vk::CommandBuffer& gBufferCommandBuffer) {
    vk::CommandBufferInheritanceInfo inheritance {
            .renderPass = *this->gRenderPass,
            .subpass = RenderPasses::GBufferSubPassIndex,
            .framebuffer = *this->swapchainFramebuffers[frameIndex],
    };
    vk::CommandBufferBeginInfo beginInfo{
            .flags = vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse,
            .pInheritanceInfo = &inheritance,
    };

    gBufferCommandBuffer.begin(beginInfo);
    {
        this->updateViewportAndScissor(gBufferCommandBuffer);

        this->game->recordGBufferPass(frameIndex, gBufferCommandBuffer);
    }
    gBufferCommandBuffer.end();
}

void Carrot::Engine::recordMainCommandBuffer(size_t i) {
    // main command buffer
    vk::CommandBufferBeginInfo beginInfo{
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
            .pInheritanceInfo = nullptr,
    };

    mainCommandBuffers[i].begin(beginInfo);
    {
        PrepareVulkanTracy(tracyCtx[i], mainCommandBuffers[i]);

        TracyVulkanZone(*tracyCtx[i], mainCommandBuffers[i], "Render frame");
        updateViewportAndScissor(mainCommandBuffers[i]);

        vk::ClearValue clearColor = vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,1.0f});
        vk::ClearValue lightingClear = vk::ClearColorValue(std::array{1.0f,1.0f,1.0f,1.0f});
        vk::ClearValue positionClear = vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,0.0f});
        vk::ClearValue uiClear = vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,0.0f});
        vk::ClearValue clearDepth = vk::ClearDepthStencilValue{
                .depth = 1.0f,
                .stencil = 0
        };

        vk::ClearValue clearValues[] = {
                clearColor, // final presented color
                clearDepth,
                clearColor, // gbuffer color
                positionClear, // gbuffer view position
                positionClear, // gbuffer view normal,
                lightingClear, // raytraced lighting colors
        };

        vk::ClearValue imguiClearValues[] = {
                uiClear, // UI contents
        };

        vk::RenderPassBeginInfo imguiRenderPassInfo {
                .renderPass = *imguiRenderPass,
                .framebuffer = *imguiFramebuffers[i],
                .renderArea = {
                        .offset = vk::Offset2D{0, 0},
                        .extent = swapchainExtent
                },

                .clearValueCount = 1,
                .pClearValues = imguiClearValues,
        };

        raytracer->recordCommands(i, mainCommandBuffers[i]);

        {
            TracyVulkanZone(*tracyCtx[i], mainCommandBuffers[i], "UI pass");
            mainCommandBuffers[i].beginRenderPass(imguiRenderPassInfo, vk::SubpassContents::eInline);
            ImGui::Render();
            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), mainCommandBuffers[i]);
            mainCommandBuffers[i].endRenderPass();
        }

        uiImages[i]->transitionLayoutInline(mainCommandBuffers[i], vk::Format::eR8G8B8A8Unorm, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

        vk::RenderPassBeginInfo gRenderPassInfo {
                .renderPass = *gRenderPass,
                .framebuffer = *swapchainFramebuffers[i],
                .renderArea = {
                        .offset = vk::Offset2D{0, 0},
                        .extent = swapchainExtent
                },

                .clearValueCount = 6,
                .pClearValues = clearValues,
        };

        static bool onlyRaytracing = true;


        {
            TracyVulkanZone(*tracyCtx[i], mainCommandBuffers[i], "Render pass 0");

            mainCommandBuffers[i].beginRenderPass(gRenderPassInfo, vk::SubpassContents::eSecondaryCommandBuffers);

            mainCommandBuffers[i].executeCommands(gBufferCommandBuffers[i]);

            mainCommandBuffers[i].nextSubpass(vk::SubpassContents::eSecondaryCommandBuffers);

            mainCommandBuffers[i].executeCommands(gResolveCommandBuffers[i]);

            mainCommandBuffers[i].endRenderPass();

        }
    }

    mainCommandBuffers[i].end();
}

void Carrot::Engine::allocateGraphicsCommandBuffers() {
    vk::CommandBufferAllocateInfo allocInfo{
            .commandPool = *this->graphicsCommandPool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = static_cast<uint32_t>(getSwapchainImageCount()),
    };

    this->mainCommandBuffers = this->device->allocateCommandBuffers(allocInfo);

    vk::CommandBufferAllocateInfo gAllocInfo {
            .commandPool = *this->graphicsCommandPool,
            .level = vk::CommandBufferLevel::eSecondary,
            .commandBufferCount = static_cast<uint32_t>(getSwapchainImageCount()),
    };
    this->gBufferCommandBuffers = device->allocateCommandBuffers(gAllocInfo);
    this->gResolveCommandBuffers = device->allocateCommandBuffers(gAllocInfo);

    vk::CommandBufferAllocateInfo uiAllocInfo {
            .commandPool = *this->graphicsCommandPool,
            .level = vk::CommandBufferLevel::eSecondary,
            .commandBufferCount = static_cast<uint32_t>(getSwapchainImageCount()),
    };
    this->uiCommandBuffers = device->allocateCommandBuffers(uiAllocInfo);
}

void Carrot::Engine::updateUniformBuffer(int imageIndex) {
    static CameraBufferObject cbo{};

    camera->updateBufferObject(cbo);
    cameraUniformBuffers[imageIndex]->directUpload(&cbo, sizeof(cbo));
}

void Carrot::Engine::drawFrame(size_t currentFrame) {
    ZoneScoped;

    vk::Result result;
    uint32_t imageIndex;
    {
        ZoneNamedN(__acquire, "Acquire image", true);
        static_cast<void>(device->waitForFences((*inFlightFences[currentFrame]), true, UINT64_MAX));
        device->resetFences((*inFlightFences[currentFrame]));


        auto nextImage = device->acquireNextImageKHR(*swapchain, UINT64_MAX, *imageAvailableSemaphore[currentFrame], nullptr);
        result = nextImage.result;

        if(result == vk::Result::eErrorOutOfDateKHR) {
            recreateSwapchain();
            return;
        } else if(result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
            throw std::runtime_error("Failed to acquire swap chain image");
        }
        imageIndex = nextImage.value;

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        static DebugBufferObject debug{};
        if(ImGui::Begin("Debug"))
        {
            ImGui::Checkbox("Show only raytracing output", &debug.onlyRaytracing);
        }
        ImGui::End();

        debugUniformBuffers[imageIndex]->directUpload(&debug, sizeof(debug));

        game->onFrame(imageIndex);

        recordMainCommandBuffer(imageIndex);
    }

    {
        ZoneScopedN("Update uniform buffer");
        updateUniformBuffer(imageIndex);
    }

/*    if(imagesInFlight[imageIndex] != nullptr) {
        device->waitForFences(*imagesInFlight[imageIndex], true, UINT64_MAX);
    }
    imagesInFlight[imageIndex] = inFlightFences[currentFrame];*/

    {
        ZoneScopedN("Present");
        vector<vk::Semaphore> waitSemaphores = {*imageAvailableSemaphore[currentFrame]};
        vector<vk::PipelineStageFlags> waitStages = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
        vk::Semaphore signalSemaphores[] = {*renderFinishedSemaphore[currentFrame]};

        game->changeGraphicsWaitSemaphores(imageIndex, waitSemaphores, waitStages);

        vk::SubmitInfo submitInfo {
                .waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size()),
                .pWaitSemaphores = waitSemaphores.data(),

                .pWaitDstStageMask = waitStages.data(),

                .commandBufferCount = 1,
                .pCommandBuffers = &mainCommandBuffers[imageIndex],

                .signalSemaphoreCount = 1,
                .pSignalSemaphores = signalSemaphores,
        };


        device->resetFences(*inFlightFences[currentFrame]);
        graphicsQueue.submit(submitInfo, *inFlightFences[currentFrame]);

        vk::SwapchainKHR swapchains[] = { *swapchain };

        vk::PresentInfoKHR presentInfo{
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = signalSemaphores,

                .swapchainCount = 1,
                .pSwapchains = swapchains,
                .pImageIndices = &imageIndex,
                .pResults = nullptr,
        };

        try {
            result = presentQueue.presentKHR(presentInfo);
        } catch(vk::OutOfDateKHRError const &e) {
            result = vk::Result::eErrorOutOfDateKHR;
        }
    }


    for(size_t i = 0; i < getSwapchainImageCount(); i++) {
        TracyVulkanCollect(tracyCtx[i]);
    }

    if(result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || framebufferResized) {
        recreateSwapchain();
    }
}

void Carrot::Engine::createSynchronizationObjects() {
    imageAvailableSemaphore.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphore.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    imagesInFlight.resize(swapchainImages.size());

    vk::SemaphoreCreateInfo semaphoreInfo{};

    vk::FenceCreateInfo fenceInfo{
            .flags = vk::FenceCreateFlagBits::eSignaled,
    };

    for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        imageAvailableSemaphore[i] = device->createSemaphoreUnique(semaphoreInfo, allocator);
        renderFinishedSemaphore[i] = device->createSemaphoreUnique(semaphoreInfo, allocator);
        inFlightFences[i] = device->createFenceUnique(fenceInfo, allocator);
    }
}

void Carrot::Engine::recreateSwapchain() {
    glfwGetFramebufferSize(window.get(), &framebufferWidth, &framebufferHeight);
    while(framebufferWidth == 0 || framebufferHeight == 0) {
        glfwGetFramebufferSize(window.get(), &framebufferWidth, &framebufferHeight);
        glfwWaitEvents();
    }

    framebufferResized = false;

    device->waitIdle();

    cleanupSwapchain();

    createSwapChain();
    createUniformBuffers();
    raytracer->onSwapchainRecreation();
    gBuffer->onSwapchainRecreation();
    createRenderPasses();
    createFramebuffers();
    gBuffer->loadResolvePipeline();
    for(const auto& [name, pipeline] : pipelines) {
        pipeline->recreateDescriptorPool(swapchainFramebuffers.size());
    }
    // TODO: update material descriptor sets
    // TODO: update game swapchain-dependent content
    allocateGraphicsCommandBuffers();
    for(size_t i = 0; i < getSwapchainImageCount(); i++) {
        recordSecondaryCommandBuffers(i); // g-buffer pass and rt pass
    }
}

void Carrot::Engine::cleanupSwapchain() {
    swapchainFramebuffers.clear();
    device->freeCommandBuffers(*graphicsCommandPool, mainCommandBuffers);
    mainCommandBuffers.clear();

    gRenderPass.reset();
    swapchainImageViews.clear();
    swapchain.reset();
}

void Carrot::Engine::onWindowResize() {
    framebufferResized = true;
}

uint32_t Carrot::Engine::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) {
    auto memProperties = physicalDevice.getMemoryProperties();
    for(uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if(typeFilter & (1 << i)
        && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw runtime_error("Failed to find suitable memory type.");
}

Carrot::QueueFamilies& Carrot::Engine::getQueueFamilies() {
    return queueFamilies;
}

vk::Device& Carrot::Engine::getLogicalDevice() {
    return *device;
}

vk::Optional<const vk::AllocationCallbacks> Carrot::Engine::getAllocator() {
    return allocator;
}

vk::CommandPool& Carrot::Engine::getTransferCommandPool() {
    return *transferCommandPool;
}

vk::CommandPool& Carrot::Engine::getGraphicsCommandPool() {
    return *graphicsCommandPool;
}

vk::Queue Carrot::Engine::getTransferQueue() {
    return transferQueue;
}

vk::Queue Carrot::Engine::getGraphicsQueue() {
    return graphicsQueue;
}

void Carrot::Engine::createUniformBuffers() {
    vk::DeviceSize bufferSize = sizeof(Carrot::CameraBufferObject);
    cameraUniformBuffers.resize(swapchainFramebuffers.size(), nullptr);

    for(size_t i = 0; i < swapchainFramebuffers.size(); i++) {
        cameraUniformBuffers[i] = make_unique<Carrot::Buffer>(*this,
                                                              bufferSize,
                                                              vk::BufferUsageFlagBits::eUniformBuffer,
                                                              vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible,
                                                              createGraphicsAndTransferFamiliesSet());
    }

    vk::DeviceSize debugBufferSize = sizeof(Carrot::DebugBufferObject);
    debugUniformBuffers.resize(swapchainFramebuffers.size(), nullptr);

    for(size_t i = 0; i < swapchainFramebuffers.size(); i++) {
        debugUniformBuffers[i] = make_unique<Carrot::Buffer>(*this,
                                                              debugBufferSize,
                                                              vk::BufferUsageFlagBits::eUniformBuffer,
                                                              vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible,
                                                              createGraphicsAndTransferFamiliesSet());
    }
}

set<uint32_t> Carrot::Engine::createGraphicsAndTransferFamiliesSet() {
    return {
        queueFamilies.graphicsFamily.value(),
        queueFamilies.transferFamily.value(),
    };
}

vk::Format Carrot::Engine::findSupportedFormat(const vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features) {
    for(auto& format : candidates) {
        vk::FormatProperties properties = physicalDevice.getFormatProperties(format);

        if(tiling == vk::ImageTiling::eLinear && (properties.linearTilingFeatures & features) == features) {
            return format;
        }

        if(tiling == vk::ImageTiling::eOptimal && (properties.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    throw runtime_error("Could not find supported format");
}

vk::Format Carrot::Engine::findDepthFormat() {
    return findSupportedFormat(
            {vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
            vk::ImageTiling::eOptimal,
            vk::FormatFeatureFlagBits::eDepthStencilAttachment);
}

void Carrot::Engine::createSamplers() {
    nearestRepeatSampler = device->createSamplerUnique({
                                                         .magFilter = vk::Filter::eNearest,
                                                         .minFilter = vk::Filter::eNearest,
                                                         .mipmapMode = vk::SamplerMipmapMode::eNearest,
                                                         .addressModeU = vk::SamplerAddressMode::eRepeat,
                                                         .addressModeV = vk::SamplerAddressMode::eRepeat,
                                                         .addressModeW = vk::SamplerAddressMode::eRepeat,
                                                         .anisotropyEnable = true,
                                                         .maxAnisotropy = 16.0f,
                                                         .unnormalizedCoordinates = false,
                                                 }, allocator);

    linearRepeatSampler = device->createSamplerUnique({
                                                        .magFilter = vk::Filter::eLinear,
                                                        .minFilter = vk::Filter::eLinear,
                                                        .mipmapMode = vk::SamplerMipmapMode::eLinear,
                                                        .addressModeU = vk::SamplerAddressMode::eRepeat,
                                                        .addressModeV = vk::SamplerAddressMode::eRepeat,
                                                        .addressModeW = vk::SamplerAddressMode::eRepeat,
                                                        .anisotropyEnable = true,
                                                        .maxAnisotropy = 16.0f,
                                                        .unnormalizedCoordinates = false,
                                                }, allocator);
}

void Carrot::Engine::updateViewportAndScissor(vk::CommandBuffer& commands) {
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

shared_ptr<Carrot::Pipeline> Carrot::Engine::getOrCreatePipeline(const string& name) {
    auto it = pipelines.find(name);
    if(it == pipelines.end()) {
        pipelines[name] = make_shared<Pipeline>(*this, gRenderPass, name);
    }
    return pipelines[name];
}

unique_ptr<Carrot::Image>& Carrot::Engine::getOrCreateTexture(const string& textureName) {
    auto it = textureImages.find(textureName);
    if(it == textureImages.end()) {
        auto texture = Image::fromFile(*this, "resources/textures/"+textureName);
        texture->name(textureName);
        textureImages[textureName] = move(texture);
    }
    return textureImages[textureName];
}

vk::UniqueImageView& Carrot::Engine::getOrCreateTextureView(const string& textureName) {
    auto it = textureImageViews.find(textureName);
    if(it == textureImageViews.end()) {
        auto& texture = getOrCreateTexture(textureName);
        auto textureView = texture->createImageView();
        textureImageViews[textureName] = move(textureView);
    }
    return textureImageViews[textureName];
}

uint32_t Carrot::Engine::getSwapchainImageCount() {
    return swapchainImages.size();
}

vector<shared_ptr<Carrot::Buffer>>& Carrot::Engine::getCameraUniformBuffers() {
    return cameraUniformBuffers;
}

vector<shared_ptr<Carrot::Buffer>>& Carrot::Engine::getDebugUniformBuffers() {
    return debugUniformBuffers;
}

const vk::UniqueSampler& Carrot::Engine::getLinearSampler() {
    return linearRepeatSampler;
}

void Carrot::Engine::createDefaultTexture() {
    defaultImage = Image::fromFile(*this, "resources/textures/default.png");
    defaultImageView = move(defaultImage->createImageView());
}

vk::UniqueHandle<vk::ImageView, vk::DispatchLoaderDynamic>& Carrot::Engine::getDefaultImageView() {
    return defaultImageView;
}

shared_ptr<Carrot::Material> Carrot::Engine::getOrCreateMaterial(const string& name) {
    auto it = materials.find(name);
    if(it == materials.end()) {
        materials[name] = make_shared<Material>(*this, name);
    }
    return materials[name];
}

void Carrot::Engine::initGame() {
    game = make_unique<Game::Game>(*this);
}

void Carrot::Engine::createCamera() {
    auto center = glm::vec3(5*0.5f, 5*0.5f, 0);

    camera = make_unique<Camera>(45.0f, swapchainExtent.width / (float) swapchainExtent.height, 0.1f, 1000.0f);
    camera->position = glm::vec3(center.x, center.y + 1, 5.0f);
    camera->target = center;
}

void Carrot::Engine::onMouseMove(double xpos, double ypos) {
    double dx = xpos-mouseX;
    double dy = ypos-mouseY;
    if(game) {
        game->onMouseMove(dx, dy);
    }
    mouseX = xpos;
    mouseY = ypos;
}

Carrot::Camera& Carrot::Engine::getCamera() {
    return *camera;
}

void Carrot::Engine::onKeyEvent(int key, int scancode, int action, int mods) {
    if(key == GLFW_KEY_ESCAPE) {
        running = false;
    }

    if(key == GLFW_KEY_G && action == GLFW_PRESS) {
        grabCursor = !grabCursor;
        glfwSetInputMode(window.get(), GLFW_CURSOR, grabCursor ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    }

    // TODO: pass input to game
}

void Carrot::Engine::createTracyContexts() {
    for(size_t i = 0; i < getSwapchainImageCount(); i++) {
        tracyCtx.emplace_back(move(make_unique<TracyVulkanContext>(physicalDevice, *device, graphicsQueue, queueFamilies.graphicsFamily.value())));
    }
}

vk::Queue Carrot::Engine::getComputeQueue() {
    return computeQueue;
}

vk::CommandPool& Carrot::Engine::getComputeCommandPool() {
    return *computeCommandPool;
}

vk::Extent2D Carrot::Engine::getSwapchainExtent() const {
    return swapchainExtent;
}

vk::Format Carrot::Engine::getDepthFormat() const {
    return depthFormat;
}

vk::Format Carrot::Engine::getSwapchainImageFormat() const {
    return swapchainImageFormat;
}

vk::PhysicalDevice& Carrot::Engine::getPhysicalDevice() {
    return physicalDevice;
}

Carrot::ASBuilder& Carrot::Engine::getASBuilder() {
    return *asBuilder;
}

vector<vk::UniqueImageView>& Carrot::Engine::getUIImageViews() {
    return uiImageViews;
}

bool Carrot::QueueFamilies::isComplete() const {
    return graphicsFamily.has_value() && presentFamily.has_value() && transferFamily.has_value() && computeFamily.has_value();
}
