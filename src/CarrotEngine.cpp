//
// Created by jglrxavpok on 21/11/2020.
//

#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <io/IO.h>
#include "CarrotEngine.h"
#include "constants.h"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) {

    //if(messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        std::cerr << "Validation layer: " << pCallbackData->pMessage << std::endl;
    //}

    return VK_FALSE;
}

CarrotEngine::CarrotEngine(NakedPtr<GLFWwindow> window): window(window) {}

void CarrotEngine::init() {
    initWindow();
    initVulkan();
}

void CarrotEngine::run() {
    init();

    size_t currentFrame = 0;

    while(running) {
        glfwPollEvents();

        if(glfwWindowShouldClose(window.get())) {
            running = false;
        }

        drawFrame(currentFrame);

        currentFrame = (currentFrame+1) % MAX_FRAMES_IN_FLIGHT;
    }

    device->waitIdle();
}

static void windowResize(GLFWwindow* window, int width, int height) {
    auto app = reinterpret_cast<CarrotEngine*>(glfwGetWindowUserPointer(window));
    app->onWindowResize();
}

void CarrotEngine::initWindow() {
    glfwSetWindowUserPointer(window.get(), this);
    glfwSetFramebufferSizeCallback(window.get(), windowResize);
}

void CarrotEngine::initVulkan() {
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
    createGraphicsPipeline();
    createFramebuffers();
    createCommandPool();
    createCommandBuffers();
    createSynchronizationObjects();
}

void CarrotEngine::createInstance() {
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

bool CarrotEngine::checkValidationLayerSupport() {
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

CarrotEngine::~CarrotEngine() {
    swapchain.reset();
    instance->destroySurfaceKHR(surface, allocator);
}

std::vector<const char *> CarrotEngine::getRequiredExtensions() {
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    // copy GLFW extensions
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if(USE_VULKAN_VALIDATION_LAYERS) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

void CarrotEngine::setupDebugMessenger() {
    if(!USE_VULKAN_VALIDATION_LAYERS) return;

    vk::DebugUtilsMessengerCreateInfoEXT createInfo{};
    setupMessenger(createInfo);

    callback = instance->createDebugUtilsMessengerEXTUnique(createInfo, allocator);
}

void CarrotEngine::setupMessenger(vk::DebugUtilsMessengerCreateInfoEXT& createInfo) {
    createInfo.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo | vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
    createInfo.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;
    createInfo.pfnUserCallback = debugCallback;
    createInfo.pUserData = nullptr;
}

void CarrotEngine::pickPhysicalDevice() {
    const std::vector<vk::PhysicalDevice> devices = instance->enumeratePhysicalDevices();

    std::multimap<int, vk::PhysicalDevice> candidates;
    for(const auto& device : devices) {
        int score = ratePhysicalDevice(device);
        candidates.insert(std::make_pair(score, device));
    }

    if(candidates.rbegin()->first > 0) { // can best candidate run this app?
        physicalDevice = candidates.rbegin()->second;
    } else {
        throw std::runtime_error("No GPU can support this application.");
    }
}

int CarrotEngine::ratePhysicalDevice(const vk::PhysicalDevice& device) {
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

QueueFamilies CarrotEngine::findQueueFamilies(vk::PhysicalDevice const &device) {
    std::vector<vk::QueueFamilyProperties> queueFamilies = device.getQueueFamilyProperties();
    uint32_t index = 0;

    QueueFamilies families;
    for(const auto& family : queueFamilies) {
        if(family.queueFlags & vk::QueueFlagBits::eGraphics) {
            families.graphicsFamily = index;
        }

        bool presentSupport = device.getSurfaceSupportKHR(index, surface);
        if(presentSupport) {
            families.presentFamily = index;
        }

        index++;
    }

    return families;
}

void CarrotEngine::createLogicalDevice() {
    QueueFamilies families = findQueueFamilies(physicalDevice);

    float priority = 1.0f;

    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfoStructs{};
    std::set<uint32_t> uniqueQueueFamilies = { families.presentFamily.value(), families.graphicsFamily.value() };

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

    graphicsQueue = device->getQueue(families.graphicsFamily.value(), 0);
    presentQueue = device->getQueue(families.presentFamily.value(), 0);
}

void CarrotEngine::createSurface() {
    auto cAllocator = (const VkAllocationCallbacks*) (allocator);
    VkSurfaceKHR cSurface;
    if(glfwCreateWindowSurface(static_cast<VkInstance>(*instance), window.get(), cAllocator, &cSurface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface.");
    }
    surface = cSurface;
}

bool CarrotEngine::checkDeviceExtensionSupport(const vk::PhysicalDevice& logicalDevice) {
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

SwapChainSupportDetails CarrotEngine::querySwapChainSupport(const vk::PhysicalDevice& device) {
    return {
            .capabilities = device.getSurfaceCapabilitiesKHR(surface),
            .formats = device.getSurfaceFormatsKHR(surface),
            .presentModes = device.getSurfacePresentModesKHR(surface),
    };
}

vk::SurfaceFormatKHR CarrotEngine::chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& formats) {
    for(const auto& available : formats) {
        if(available.format == vk::Format::eA8B8G8R8SrgbPack32 && available.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            return available;
        }
    }

    // TODO: rank based on format and color space

    return formats[0];
}

vk::PresentModeKHR CarrotEngine::chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& presentModes) {
    for(const auto& mode : presentModes) {
        if(mode == vk::PresentModeKHR::eMailbox) {
            return mode;
        }
    }

    // only one guaranteed
    return vk::PresentModeKHR::eFifo;
}

vk::Extent2D CarrotEngine::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities) {
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

void CarrotEngine::createSwapChain() {
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

void CarrotEngine::createSwapChainImageViews() {
    swapchainImageViews.resize(swapchainImages.size());

    for(size_t index = 0; index < swapchainImages.size(); index++) {
        auto view = CarrotEngine::createImageView(swapchainImages[index], swapchainImageFormat);
        swapchainImageViews[index] = std::move(view);
    }
}

vk::UniqueImageView CarrotEngine::createImageView(const vk::Image& image, vk::Format imageFormat, vk::ImageAspectFlagBits aspectMask) const {
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

void CarrotEngine::createGraphicsPipeline() {
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

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
            .vertexBindingDescriptionCount = 0,
            .pVertexBindingDescriptions = nullptr,

            .vertexAttributeDescriptionCount = 0,
            .pVertexAttributeDescriptions = nullptr,
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

            .cullMode = vk::CullModeFlagBits::eBack,
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
            .setLayoutCount = 0,
            .pSetLayouts = nullptr,
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

vk::UniqueShaderModule CarrotEngine::createShaderModule(const std::vector<char>& bytecode) {
    return device->createShaderModuleUnique({
                                                    .codeSize = bytecode.size(),
                                                    .pCode = reinterpret_cast<const uint32_t*>(bytecode.data()),
                                            }, allocator);
}

void CarrotEngine::createRenderPass() {
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

void CarrotEngine::createFramebuffers() {
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

void CarrotEngine::createCommandPool() {
    QueueFamilies families = findQueueFamilies(physicalDevice);

    vk::CommandPoolCreateInfo poolInfo{
            .queueFamilyIndex = families.graphicsFamily.value(),
            // .flags = <value>,  // TODO: resettable command buffers
    };

    commandPool = device->createCommandPoolUnique(poolInfo, allocator);
}

void CarrotEngine::createCommandBuffers() {
    vk::CommandBufferAllocateInfo allocInfo{
            .commandPool = *commandPool,
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

        commandBuffers[i].draw(3, 1, 0, 0);

        commandBuffers[i].endRenderPass();

        commandBuffers[i].end();
    }
}

void CarrotEngine::drawFrame(size_t currentFrame) {
    static_cast<void>(device->waitForFences((*inFlightFences[currentFrame]), true, UINT64_MAX));
    device->resetFences((*inFlightFences[currentFrame]));

    auto [result, imageIndex] = device->acquireNextImageKHR(*swapchain, UINT64_MAX, *imageAvailableSemaphore[currentFrame], nullptr);
    if(result == vk::Result::eErrorOutOfDateKHR) {
        recreateSwapchain();
        return;
    } else if(result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
        throw std::runtime_error("Failed to acquire swap chain image");
    }

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

void CarrotEngine::createSynchronizationObjects() {
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

void CarrotEngine::recreateSwapchain() {
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
    createCommandBuffers();
}

void CarrotEngine::cleanupSwapchain() {
    swapchainFramebuffers.clear();
    device->freeCommandBuffers(*commandPool, commandBuffers);
    commandBuffers.clear();

    graphicsPipeline.reset();
    pipelineLayout.reset();
    renderPass.reset();
    swapchainImageViews.clear();
    swapchain.reset();
}

void CarrotEngine::onWindowResize() {
    framebufferResized = true;
}

bool QueueFamilies::isComplete() {
    return graphicsFamily.has_value() && presentFamily.has_value();
}
