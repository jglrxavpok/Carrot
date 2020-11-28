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
#include <render/UniformBufferObject.h>
#include "Engine.h"
#include "constants.h"
#include "render/Buffer.h"
#include "render/Vertex.h"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

const std::vector<Carrot::Vertex> vertices = {
        {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        {{0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        {{0.5f, 0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}}
};

const std::vector<uint32_t> indices = {
        0,1,2,
        2,3,0,
};

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) {

    if(messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        std::cerr << "Validation layer: " << pCallbackData->pMessage << std::endl;
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
            glfwHideWindow(window.get());
            running = false;
        }

        drawFrame(currentFrame);

        currentFrame = (currentFrame+1) % MAX_FRAMES_IN_FLIGHT;
    }

    device->waitIdle();
}

static void windowResize(GLFWwindow* window, int width, int height) {
    auto app = reinterpret_cast<Carrot::Engine*>(glfwGetWindowUserPointer(window));
    app->onWindowResize();
}

void Carrot::Engine::initWindow() {
    glfwSetWindowUserPointer(window.get(), this);
    glfwSetFramebufferSizeCallback(window.get(), windowResize);
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
    createRenderPass();
    createDescriptorSetLayout();
    createGraphicsPipeline();
    createFramebuffers();
    createGraphicsCommandPool();
    createTransferCommandPool();
    createVertexBuffer();
    createIndexBuffer();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    createCommandBuffers();
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

    // prefer maximum texture size
    score += deviceProperties.limits.maxImageDimension2D;

    if(!deviceFeatures.geometryShader) { // must support geometry shaders
        return 0;
    }

    return score;
}

Carrot::QueueFamilies Carrot::Engine::findQueueFamilies(vk::PhysicalDevice const &device) {
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

        bool presentSupport = device.getSurfaceSupportKHR(index, surface);
        if(presentSupport) {
            families.presentFamily = index;
        }

        index++;
    }

    // graphics queue implicitly support transfer operations
    if(!families.transferFamily.has_value()) {
        families.transferFamily = families.graphicsFamily;
    }

    return families;
}

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
    vk::PhysicalDeviceFeatures deviceFeatures{};

    vk::DeviceCreateInfo createInfo{
            .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfoStructs.size()),
            .pQueueCreateInfos = queueCreateInfoStructs.data(),
            .enabledExtensionCount = static_cast<uint32_t>(VULKAN_DEVICE_EXTENSIONS.size()),
            .ppEnabledExtensionNames = VULKAN_DEVICE_EXTENSIONS.data(),
            .pEnabledFeatures = &deviceFeatures,
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

    createSwapChainImageViews();
}

void Carrot::Engine::createSwapChainImageViews() {
    swapchainImageViews.resize(swapchainImages.size());

    for(size_t index = 0; index < swapchainImages.size(); index++) {
        auto view = Engine::createImageView(swapchainImages[index], swapchainImageFormat);
        swapchainImageViews[index] = std::move(view);
    }
}

vk::UniqueImageView Carrot::Engine::createImageView(const vk::Image& image, vk::Format imageFormat, vk::ImageAspectFlagBits aspectMask) const {
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

void Carrot::Engine::createGraphicsPipeline() {
    auto vertexCode = IO::readFile("resources/shaders/default.vertex.glsl.spv");
    auto fragmentCode = IO::readFile("resources/shaders/default.fragment.glsl.spv");

    vk::UniqueShaderModule vertexShader = createShaderModule(vertexCode);
    vk::UniqueShaderModule fragmentShader = createShaderModule(fragmentCode);

    vk::PipelineShaderStageCreateInfo shaderStages[] = { {
                                                                 .stage = vk::ShaderStageFlagBits::eVertex,
                                                                 .module = *vertexShader,
                                                                 .pName = "main",
                                                         },
                                                         {
                                                                 .stage = vk::ShaderStageFlagBits::eFragment,
                                                                 .module = *fragmentShader,
                                                                 .pName = "main",
                                                         }};

    auto bindingDescription = Carrot::Vertex::getBindingDescription();
    auto attributeDescriptions = Carrot::Vertex::getAttributeDescriptions();

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = &bindingDescription,

            .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
            .pVertexAttributeDescriptions = attributeDescriptions.data(),
    };
    // TODO: vertex buffers

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
            .topology = vk::PrimitiveTopology::eTriangleList,
            .primitiveRestartEnable = false,
    };

    vk::Viewport viewport{
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>(swapchainExtent.width),
            .height = static_cast<float>(swapchainExtent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
    };

    vk::Rect2D scissor{
            .offset = vk::Offset2D{0, 0},
            .extent = swapchainExtent,
    };

    vk::PipelineViewportStateCreateInfo viewportState{
            .viewportCount = 1,
            .pViewports = &viewport,

            .scissorCount = 1,
            .pScissors = &scissor,
    };

    vk::PipelineRasterizationStateCreateInfo rasterizer{
            // TODO: change for shadow maps
            .depthClampEnable = false,

            .rasterizerDiscardEnable = false,

            .polygonMode = vk::PolygonMode::eFill,

            .cullMode = vk::CullModeFlagBits::eFront,
            .frontFace = vk::FrontFace::eClockwise,

            // TODO: change for shadow maps
            .depthBiasEnable = false,
            .depthBiasConstantFactor = 0.0f,
            .depthBiasClamp = 0.0f,
            .depthBiasSlopeFactor = 0.0f,

            .lineWidth = 1.0f,
    };

    vk::PipelineMultisampleStateCreateInfo multisampling{
            .rasterizationSamples = vk::SampleCountFlagBits::e1,
            .sampleShadingEnable = false,
            .minSampleShading = 1.0f,
            .pSampleMask = nullptr,
            .alphaToCoverageEnable = false,
            .alphaToOneEnable = false,
    };
    // TODO: Depth & stencil

    vk::PipelineColorBlendAttachmentState colorBlendAttachment{
            // TODO: blending
            .blendEnable = false,

            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
    };

    vk::PipelineColorBlendStateCreateInfo colorBlending{
            .logicOpEnable = false,
            .attachmentCount = 1,
            .pAttachments = &colorBlendAttachment,
    };

    // TODO: dynamic state

    vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo{
            .setLayoutCount = 1,
            .pSetLayouts = &(*descriptorSetLayout),
            .pushConstantRangeCount = 0,
            .pPushConstantRanges = nullptr,
    };

    pipelineLayout = device->createPipelineLayoutUnique(pipelineLayoutCreateInfo, allocator);

    vk::GraphicsPipelineCreateInfo pipelineInfo{
            .stageCount = 2,
            .pStages = shaderStages,

            .pVertexInputState = &vertexInputInfo,
            .pInputAssemblyState = &inputAssembly,
            .pViewportState = &viewportState,
            .pRasterizationState = &rasterizer,
            .pMultisampleState = &multisampling,
            .pDepthStencilState = nullptr,
            .pColorBlendState = &colorBlending,
            .pDynamicState = nullptr,

            .layout = *pipelineLayout,
            .renderPass = *renderPass,
            .subpass = 0,
    };

    graphicsPipeline = device->createGraphicsPipelineUnique(nullptr, pipelineInfo, allocator);
    // modules will be destroyed after the end of this function
}

vk::UniqueShaderModule Carrot::Engine::createShaderModule(const std::vector<char>& bytecode) {
    return device->createShaderModuleUnique({
                                                    .codeSize = bytecode.size(),
                                                    .pCode = reinterpret_cast<const uint32_t*>(bytecode.data()),
                                            }, allocator);
}

void Carrot::Engine::createRenderPass() {
    vk::AttachmentDescription colorAttachment{
            .format = swapchainImageFormat,
            .samples = vk::SampleCountFlagBits::e1,

            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,

            .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
            .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,

            .initialLayout = vk::ImageLayout::eUndefined,
            .finalLayout = vk::ImageLayout::ePresentSrcKHR,
    };

    vk::AttachmentReference colorAttachmentRef{
            .attachment = 0,
            .layout = vk::ImageLayout::eColorAttachmentOptimal,
    };

    vk::SubpassDescription subpass{
            .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
            .inputAttachmentCount = 0,

            .colorAttachmentCount = 1,
            // index in this array is used by `layout(location = 0)` inside shaders
            .pColorAttachments = &colorAttachmentRef,
            .pDepthStencilAttachment = nullptr,

            .preserveAttachmentCount = 0,
    };

    vk::SubpassDependency dependency{
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0, // our subpass

            .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
            // TODO: .srcAccessMask = 0,

            .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
            .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
    };

    vk::RenderPassCreateInfo renderPassInfo{
            .attachmentCount = 1,
            .pAttachments = &colorAttachment,
            .subpassCount = 1,
            .pSubpasses = &subpass,

            .dependencyCount = 1,
            .pDependencies = &dependency,
    };

    renderPass = device->createRenderPassUnique(renderPassInfo, allocator);
}

void Carrot::Engine::createFramebuffers() {
    swapchainFramebuffers.resize(swapchainImages.size());

    for (size_t i = 0; i < swapchainImageViews.size(); ++i) {
        vk::ImageView attachments[] = {
                *swapchainImageViews[i]
        };

        vk::FramebufferCreateInfo framebufferInfo{
                .renderPass = *renderPass,
                .attachmentCount = 1,
                .pAttachments = attachments,
                .width = swapchainExtent.width,
                .height = swapchainExtent.height,
                .layers = 1,
        };

        swapchainFramebuffers[i] = std::move(device->createFramebufferUnique(framebufferInfo, allocator));
    }
}

void Carrot::Engine::createGraphicsCommandPool() {
    vk::CommandPoolCreateInfo poolInfo{
            .queueFamilyIndex = queueFamilies.graphicsFamily.value(),
            // .flags = <value>,  // TODO: resettable command buffers
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

void Carrot::Engine::createCommandBuffers() {
    vk::CommandBufferAllocateInfo allocInfo{
            .commandPool = *graphicsCommandPool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = static_cast<uint32_t>(swapchainFramebuffers.size()),
    };

    commandBuffers = device->allocateCommandBuffers(allocInfo);

    for(size_t i = 0; i < commandBuffers.size(); i++) {
        vk::CommandBufferBeginInfo beginInfo{
                .pInheritanceInfo = nullptr,
                // TODO: different flags: .flags = vk::CommandBufferUsageFlagBits::<value>
        };

        commandBuffers[i].begin(beginInfo);

        vk::ClearValue clearColor = vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,1.0f});

        vk::RenderPassBeginInfo renderPassInfo{
                .renderPass = *renderPass,
                .framebuffer = *swapchainFramebuffers[i],
                .renderArea = {
                        .offset = vk::Offset2D{0, 0},
                        .extent = swapchainExtent
                },

                .clearValueCount = 1,
                .pClearValues = &clearColor,
        };

        commandBuffers[i].beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

        commandBuffers[i].bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);

        commandBuffers[i].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0, *descriptorSets[i], {0});
        commandBuffers[i].bindIndexBuffer(indexBuffer->getVulkanBuffer(), 0, vk::IndexType::eUint32);
        commandBuffers[i].bindVertexBuffers(0, vertexBuffer->getVulkanBuffer(), {0});
        commandBuffers[i].drawIndexed(indices.size(), 1, 0, 0, 0);

        commandBuffers[i].endRenderPass();

        commandBuffers[i].end();
    }
}

void Carrot::Engine::updateUniformBuffer(int imageIndex) {
    static UniformBufferObject ubo{};
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
    ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.projection = glm::perspective(glm::radians(45.0f), swapchainExtent.width / (float) swapchainExtent.height, 0.1f, 10.0f);
    ubo.projection[1][1] *= -1; // convert to Vulkan coordinates (from OpenGL)

    uniformBuffers[imageIndex]->directUpload(&ubo, sizeof(ubo));
}

void Carrot::Engine::drawFrame(size_t currentFrame) {
    static_cast<void>(device->waitForFences((*inFlightFences[currentFrame]), true, UINT64_MAX));
    device->resetFences((*inFlightFences[currentFrame]));

    auto [result, imageIndex] = device->acquireNextImageKHR(*swapchain, UINT64_MAX, *imageAvailableSemaphore[currentFrame], nullptr);
    if(result == vk::Result::eErrorOutOfDateKHR) {
        recreateSwapchain();
        return;
    } else if(result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
        throw std::runtime_error("Failed to acquire swap chain image");
    }

    updateUniformBuffer(imageIndex);

/*    if(imagesInFlight[imageIndex] != nullptr) {
        device->waitForFences(*imagesInFlight[imageIndex], true, UINT64_MAX);
    }
    imagesInFlight[imageIndex] = inFlightFences[currentFrame];*/

    vk::Semaphore waitSemaphores[] = {*imageAvailableSemaphore[currentFrame]};
    vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
    vk::Semaphore signalSemaphores[] = {*renderFinishedSemaphore[currentFrame]};

    vk::SubmitInfo submitInfo {
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = waitSemaphores,

            .pWaitDstStageMask = waitStages,

            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffers[imageIndex],

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
    int w, h;
    glfwGetFramebufferSize(window.get(), &w, &h);
    while(w == 0 || h == 0) {
        glfwGetFramebufferSize(window.get(), &w, &h);
        glfwWaitEvents();
    }

    framebufferResized = false;

    device->waitIdle();

    cleanupSwapchain();

    createSwapChain();
    createSwapChainImageViews();
    createRenderPass();
    createGraphicsPipeline();
    createFramebuffers();
    createUniformBuffers();
    createDescriptorSets();
    createCommandBuffers();
}

void Carrot::Engine::cleanupSwapchain() {
    swapchainFramebuffers.clear();
    device->freeCommandBuffers(*graphicsCommandPool, commandBuffers);
    commandBuffers.clear();

    graphicsPipeline.reset();
    pipelineLayout.reset();
    renderPass.reset();
    swapchainImageViews.clear();
    swapchain.reset();
}

void Carrot::Engine::onWindowResize() {
    framebufferResized = true;
}

void Carrot::Engine::createVertexBuffer() {
    std::set<uint32_t> families = {
            queueFamilies.transferFamily.value(), queueFamilies.graphicsFamily.value()
    };
    vertexBuffer = make_unique<Carrot::Buffer>(*this,
                                               sizeof(Carrot::Vertex) * vertices.size(),
                                               vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
                                               vk::MemoryPropertyFlagBits::eDeviceLocal,
                                               families);

    vertexBuffer->stageUpload(vertices);
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

vk::Queue& Carrot::Engine::getTransferQueue() {
    return transferQueue;
}

void Carrot::Engine::createIndexBuffer() {
    set<uint32_t> families = {
            queueFamilies.transferFamily.value(),
            queueFamilies.graphicsFamily.value(),
    };
    indexBuffer = make_unique<Carrot::Buffer>(*this,
                                              sizeof(uint32_t) * indices.size(),
                                              vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
                                              vk::MemoryPropertyFlagBits::eDeviceLocal,
                                              families);
    indexBuffer->stageUpload(indices);
}

void Carrot::Engine::createDescriptorSetLayout() {
    vk::DescriptorSetLayoutBinding uboBinding{
        .binding = 0, // TODO: customizable
        .descriptorType = vk::DescriptorType::eUniformBufferDynamic,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eVertex,
    };

    vk::DescriptorSetLayoutCreateInfo createInfo{
        .bindingCount = 1,
        .pBindings = &uboBinding,
    };

    descriptorSetLayout = device->createDescriptorSetLayoutUnique(createInfo, allocator);
}

void Carrot::Engine::createUniformBuffers() {
    vk::DeviceSize bufferSize = sizeof(Carrot::UniformBufferObject);
    uniformBuffers.resize(swapchainFramebuffers.size(), nullptr);

    for(size_t i = 0; i < swapchainFramebuffers.size(); i++) {
        uniformBuffers[i] = make_unique<Carrot::Buffer>(*this,
                                                        bufferSize,
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

void Carrot::Engine::createDescriptorSets() {
    vector<vk::DescriptorSetLayout> layouts{static_cast<uint32_t>(swapchainFramebuffers.size()), *descriptorSetLayout};
    vk::DescriptorSetAllocateInfo allocInfo{
        .descriptorPool = *descriptorPool,
        .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
        .pSetLayouts = layouts.data(),
    };

    descriptorSets = device->allocateDescriptorSetsUnique(allocInfo);

    vector<vk::WriteDescriptorSet> writes{swapchainFramebuffers.size()};
    for(size_t i = 0; i < swapchainFramebuffers.size(); i++) {
        vk::DescriptorBufferInfo bufferInfo = {
                .buffer = uniformBuffers[i]->getVulkanBuffer(),
                .offset = 0,
                .range = sizeof(UniformBufferObject),
        };

        writes[i] = {
                .dstSet = *descriptorSets[i],
                .dstBinding = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eUniformBufferDynamic,
                .pBufferInfo = &bufferInfo,
        };
    }
    device->updateDescriptorSets(writes, {});
}

void Carrot::Engine::createDescriptorPool() {
    vk::DescriptorPoolSize size{
        .type = vk::DescriptorType::eUniformBufferDynamic,
        .descriptorCount = static_cast<uint32_t>(swapchainFramebuffers.size()),
    };

    vk::DescriptorPoolCreateInfo poolInfo{
        .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        .maxSets = static_cast<uint32_t>(swapchainFramebuffers.size()),
        .poolSizeCount = 1,
        .pPoolSizes = &size,
    };

    descriptorPool = device->createDescriptorPoolUnique(poolInfo, allocator);
}

bool Carrot::QueueFamilies::isComplete() {
    return graphicsFamily.has_value() && presentFamily.has_value() && transferFamily.has_value();
}
