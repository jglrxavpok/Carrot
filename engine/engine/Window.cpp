//
// Created by jglrxavpok on 17/09/2021.
//

#include "Window.h"
#include "core/exceptions/Exceptions.h"
#include <list>
#include <vector>
#include <stb_image.h>
#include <engine/vulkan/VulkanDriver.h>
#include <engine/utils/Macros.h>
#include <engine/Engine.h>
#include <engine/render/resources/Texture.h>

namespace Carrot {
    static std::atomic<WindowID> WindowIDCounter = 0;

    Window::Window(Engine& engine): engine(engine) {
        windowID = ++WindowIDCounter;
    }

    Window::Window(Engine& engine, std::uint32_t width, std::uint32_t height, Configuration config): Window(engine) {
        if(!glfwInit()) {
            throw Carrot::Exceptions::IrrecoverableError("Could not init GLFW");
        }
        ownsWindow = true;

        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        if(config.startInFullscreen) {
            GLFWmonitor* primary = glfwGetPrimaryMonitor();
            const GLFWvidmode* videoMode = glfwGetVideoMode(primary);
            glfwWindow = glfwCreateWindow(videoMode->width, videoMode->height, config.applicationName.c_str(), primary, nullptr);
        } else {
            glfwWindow = glfwCreateWindow(width, height, config.applicationName.c_str(), nullptr, nullptr);
        }

        glfwSetWindowUserPointer(glfwWindow, this);

        if(config.icon16.has_value()) {
            set16x16Icon(*config.icon16);
        }
        if(config.icon32.has_value()) {
            set32x32Icon(*config.icon32);
        }
        if(config.icon64.has_value()) {
            set64x64Icon(*config.icon64);
        }
        if(config.icon128.has_value()) {
            set128x128Icon(*config.icon128);
        }
    }

    Window::Window(Engine& engine, GLFWwindow* pWindow): Window(engine) {
        glfwWindow = pWindow;
        ownsWindow = false;

        glfwSetWindowUserPointer(glfwWindow, this);
    }

    Window::Window(Engine& engine, vk::SurfaceKHR surface): Window(engine) {
        glfwWindow = nullptr;
        ownsWindow = false;
        this->surface = surface;
    }

    void Window::computeSwapchainOffset() {
        // ensure we know the proper offset for the next acquire: the renderer assumes all windows have the same swapchain index,
        //  we just fix up the index in Window with this offset
        swapchainIndexOffset = (2-engine.getSwapchainImageIndexRightNow()) % engine.getSwapchainImageCount();
    }

    void Window::createSurface() {
        ownsSurface = true;
        // create surface
        VkSurfaceKHR cSurface;
        if (glfwCreateWindowSurface(static_cast<VkInstance>(GetVulkanDriver().getInstance()), glfwWindow, nullptr,
                                    &cSurface) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create window surface.");
        }
        surface = cSurface;
    }

    void Window::createSwapChain() {
        if(imageAvailableSemaphores.empty()) {
            vk::SemaphoreCreateInfo semaphoreInfo{};
            imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
            for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                imageAvailableSemaphores[i] = engine.getLogicalDevice().createSemaphoreUnique(semaphoreInfo, engine.getVulkanDriver().getAllocationCallbacks());
            }
        }

        SwapChainSupportDetails swapChainSupportDetails = getSwapChainSupport(GetVulkanDriver().getPhysicalDevice());

        vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupportDetails.formats);
        vk::PresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupportDetails.presentModes);
        vk::Extent2D swapchainExtent = chooseSwapExtent(swapChainSupportDetails.capabilities);

        uint32_t imageCount = swapChainSupportDetails.capabilities.minImageCount +1;
        // maxImageCount == 0 means we can request any number of image
        if(swapChainSupportDetails.capabilities.maxImageCount > 0 && imageCount > swapChainSupportDetails.capabilities.maxImageCount) {
            // ensure we don't ask for more images than the device will be able to provide
            imageCount = swapChainSupportDetails.capabilities.maxImageCount;
        }

        if(!isMainWindow()) {
            verify(imageCount == GetEngine().getSwapchainImageCount(), "All windows are expected to have the same number of swapchain images. This engine does not support other cases in its current state");
        }

        vk::SwapchainCreateInfoKHR createInfo{
                .surface = getSurface(),
                .minImageCount = imageCount,
                .imageFormat = surfaceFormat.format,
                .imageColorSpace = surfaceFormat.colorSpace,
                .imageExtent = swapchainExtent,
                .imageArrayLayers = 1,
                .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst, // used for rendering or blit

                .preTransform = swapChainSupportDetails.capabilities.currentTransform,

                // don't try to blend with background of other windows
                .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,

                .presentMode = presentMode,
                .clipped = VK_TRUE,

                .oldSwapchain = *swapchain,
        };

        // image info
        auto& driver = engine.getVulkanDriver();
        uint32_t indices[] = { driver.getQueueFamilies().graphicsFamily.value(), driver.getQueueFamilies().presentFamily.value() };

        if(driver.getQueueFamilies().presentFamily != driver.getQueueFamilies().graphicsFamily) {
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

        auto& vkDevice = engine.getLogicalDevice();
        swapchain = vkDevice.createSwapchainKHRUnique(createInfo, driver.getAllocationCallbacks());

        swapchainImageFormat = surfaceFormat.format;
        framebufferExtent = swapchainExtent;

        const auto& swapchainDeviceImages = vkDevice.getSwapchainImagesKHR(*swapchain);
        swapchainTextures.clear();
        for(const auto& image : swapchainDeviceImages) {
            swapchainTextures.emplace_back(std::make_shared<Carrot::Render::Texture>(driver, image, vk::Extent3D {
                    .width = swapchainExtent.width,
                    .height = swapchainExtent.height,
                    .depth = 1,
            }, swapchainImageFormat));
        }
    }

    void Window::cleanupSwapChain() {
        swapchainTextures.clear();
        swapchain.reset();
    }

    void Window::destroySwapchainAndSurface() {
        swapchain.reset();
        if(ownsSurface) {
            engine.getVulkanDriver().getInstance().destroySurfaceKHR(getSurface(), engine.getVulkanDriver().getAllocationCallbacks());
            // TODO: don't destroy device if in VR
        }
    }

    Window::~Window() {
        if(!glfwWindow || !ownsWindow) {
            return;
        }

        glfwDestroyWindow(glfwWindow);
        glfwWindow = nullptr;
        glfwTerminate();
    }

    void Window::setCurrentSwapchainIndex(std::int32_t mainWindowSwapchainIndex, std::int32_t thisWindowSwapchainIndex) {
        std::int32_t swapchainOffset = thisWindowSwapchainIndex - mainWindowSwapchainIndex;
        while(swapchainOffset < 0) {
            swapchainOffset += GetEngine().getSwapchainImageCount();
        }

        if(!acquiredAtLeastOneImage) {
            verify(swapchainIndexOffset == swapchainOffset, "Swapchain index offset is not the value that was expected. It does not match with offset computed on construction of this window.");
        }
        swapchainIndexOffset = swapchainOffset;
        swapchainIndex = thisWindowSwapchainIndex;

        acquiredAtLeastOneImage = true;
    }

    std::size_t Window::getSwapchainIndexOffset() const {
        return swapchainIndexOffset;
    }

    bool Window::hasAcquiredAtLeastOneImage() const {
        return acquiredAtLeastOneImage;
    }

    bool Window::isFirstPresent() const {
        return firstPresent;
    }

    void Window::clearFirstPresent() {
        firstPresent = false;
    }

    vk::Semaphore Window::getImageAvailableSemaphore(std::size_t frameInFlightIndex) {
        verify(!imageAvailableSemaphores.empty(), "Called too early!");
        return *imageAvailableSemaphores[frameInFlightIndex];
    }

    Carrot::SwapChainSupportDetails Window::getSwapChainSupport(const vk::PhysicalDevice& device) {
        return {
            .capabilities = device.getSurfaceCapabilitiesKHR(surface),
            .formats = device.getSurfaceFormatsKHR(surface),
            .presentModes = device.getSurfacePresentModesKHR(surface),
        };
    }

    size_t Window::getSwapchainImageCount() const {
        return swapchainTextures.size();
    }

    const vk::Format& Window::getSwapchainImageFormat() const {
        return swapchainImageFormat;
    }

    const vk::Extent2D& Window::getFramebufferExtent() const {
        return framebufferExtent;
    }

    const vk::SwapchainKHR& Window::getSwapchain() const {
        return *swapchain;
    }

    std::shared_ptr<Render::Texture> Window::getSwapchainTexture(std::size_t swapchainIndex) {
        return swapchainTextures[(swapchainIndex + swapchainIndexOffset) % GetEngine().getSwapchainImageCount()];
    }

    void Window::fetchNewFramebufferSize() {
        if(!glfwWindow) {
            return;
        }
        std::int32_t w, h;
        getFramebufferSize(w, h);
        while(w == 0 || h == 0) {
            getFramebufferSize(w, h);
            glfwWaitEvents();
        }
    }

    void Window::updateIcons() {
        std::vector<std::uint8_t*> iconPixels{icons.size(), nullptr};
        std::vector<GLFWimage> iconImages{icons.size()};

        std::uint32_t i = 0;
        for (const auto& icon : icons) {
            int w, h, n;
            auto buffer = icon.image.readAll();
            iconPixels[i] = stbi_load_from_memory(buffer.get(), icon.image.getSize(), &w, &h, &n, 4);
            assert(w == icon.width);
            assert(h == icon.height);
            iconImages[i] = GLFWimage {
                    .width = static_cast<int>(icon.width),
                    .height = static_cast<int>(icon.height),
                    .pixels = iconPixels[i],
            };

            i++;
        }

        glfwSetWindowIcon(glfwWindow, static_cast<int>(iconImages.size()), iconImages.data());

        for(auto* ptr : iconPixels) {
            stbi_image_free(ptr);
        }
    }

    void Window::setIcon(std::uint32_t size, const Carrot::IO::Resource& resource) {
        auto memcopy = resource.copyToMemory();
        icons.emplace_back(std::move(Icon {
            .width = size,
            .height = size,
            .image = std::move(memcopy)
        }));

        updateIcons();
    }

    void Window::set16x16Icon(const Carrot::IO::Resource& icon) {
        setIcon(16, icon);
    }

    void Window::set32x32Icon(const Carrot::IO::Resource& icon) {
        setIcon(32, icon);
    }

    void Window::set64x64Icon(const Carrot::IO::Resource& icon) {
        setIcon(64, icon);
    }

    void Window::set128x128Icon(const Carrot::IO::Resource& icon) {
        setIcon(128, icon);
    }

    void Window::setTitle(std::string_view title) {
        glfwSetWindowTitle(glfwWindow, title.data());
    }

    void Window::getFramebufferSize(std::int32_t& w, std::int32_t& h) const {
        int width, height;
        glfwGetFramebufferSize(glfwWindow, &width, &height);
        w = width;
        h = height;
    }

    void Window::maximise() {
        glfwMaximizeWindow(glfwWindow);
    }

    bool Window::isMaximised() const {
        return glfwGetWindowAttrib(glfwWindow, GLFW_MAXIMIZED) == GLFW_TRUE;
    }

    bool Window::isMainWindow() const {
        return mainWindow;
    }

    vk::SurfaceKHR Window::getSurface() const {
        return surface;
    }

    void Window::getWindowSize(std::int32_t& w, std::int32_t& h) const {
        int width, height;
        glfwGetWindowSize(glfwWindow, &width, &height);
        w = width;
        h = height;
    }

    void Window::getPosition(std::int32_t& x, std::int32_t& y) const {
        int xPos, yPos;
        glfwGetWindowPos(glfwWindow, &xPos, &yPos);
        x = xPos;
        y = yPos;
    }

    void Window::setPosition(std::int32_t x, std::int32_t y) {
        glfwSetWindowPos(glfwWindow, x, y);
    }

    void Window::setWindowSize(std::int32_t w, std::int32_t h) {
        if(glfwWindow != nullptr) {
            glfwSetWindowSize(glfwWindow, w, h);
        } else {
            // adopted windows (eg ImGui)
            framebufferExtent.width = w;
            framebufferExtent.height = h;

            acquiredAtLeastOneImage = false;
            firstPresent = true;
        }
    }

    void Window::setMainWindow() {
        swapchainIndexOffset = 0;
        mainWindow = true;
    }

    WindowID Window::getWindowID() const {
        return windowID;
    }

    bool Window::operator==(const Window& other) const {
        return windowID == other.windowID;
    }

    bool Window::operator==(const WindowID& otherID) const {
        return windowID == otherID;
    }

    vk::Extent2D Window::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities) {
        if(capabilities.currentExtent.width != UINT32_MAX) {
            return capabilities.currentExtent; // no choice
        } else {
            int width, height;
            glfwGetFramebufferSize(glfwWindow, &width, &height);

            vk::Extent2D actualExtent = {
                    static_cast<uint32_t>(width),
                    static_cast<uint32_t>(height),
            };

            actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
            actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

            return actualExtent;
        }
    }

    vk::SurfaceFormatKHR Window::chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& formats) {
        for(const auto& available : formats) {
            if(available.format == vk::Format::eA8B8G8R8SrgbPack32 && available.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
                return available;
            }
        }

        // TODO: rank based on format and color space

        return formats[0];
    }

    vk::PresentModeKHR Window::chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& presentModes) {
        // TODO: don't use Mailbox for VSync
        for(const auto& mode : presentModes) {
            if(mode == vk::PresentModeKHR::eMailbox) {
                return mode;
            }
        }

        // only one guaranteed
        return vk::PresentModeKHR::eFifo;
    }
}