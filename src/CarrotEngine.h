//
// Created by jglrxavpok on 21/11/2020.
//
#pragma once
#include <memory>
#include <optional>
#include <vector>
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include "memory/NakedPtr.hpp"
#include "vulkan/types.h"

using namespace std;

struct QueueFamilies {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete();
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class CarrotEngine {
public:
    CarrotEngine() = default;
    void run();

private:
    bool running = true;
    NakedPtr<GLFWwindow> window = nullptr;
    NakedPtr<VkAllocationCallbacks> allocator = nullptr;

    VkInstance instance{};
    VkDebugUtilsMessengerEXT callback{};
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    std::unique_ptr<Swapchain> swapchain = nullptr;
    std::vector<VkImage> swapchainImages{};
    VkFormat swapchainImageFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent{};
    std::vector<std::unique_ptr<ImageView>> swapchainImageViews{};

    /// Init engine
    void init();

    /// Init window
    void initWindow();

    /// Init Vulkan for rendering
    void initVulkan();

    /// Cleanup resources
    void cleanup();

    /// Create Vulkan instance
    void createInstance();

    /// Check validation layers are supported (if NO_DEBUG disabled)
    bool checkValidationLayerSupport();

    /// Generate a vector with the required extensions for this application
    std::vector<const char *> getRequiredExtensions();

    /// Setups debug messages
    void setupDebugMessenger();

    /// Prepares a debug messenger
    void setupMessenger(VkDebugUtilsMessengerCreateInfoEXT &ext);

    /// Select a GPU
    void pickPhysicalDevice();

    /// Rank physical device based on their capabilities
    int ratePhysicalDevice(const VkPhysicalDevice& device);

    /// Gets the queue indices of a given physical device
    QueueFamilies findQueueFamilies(const VkPhysicalDevice& device);

    /// Create the logical device to interface with Vulkan
    void createLogicalDevice();

    /// Create the rendering surface for Vulkan
    void createSurface();

    /// Check the given device supports the extensions inside VULKAN_DEVICE_EXTENSIONS (constants.h)
    bool checkDeviceExtensionSupport(const VkPhysicalDevice &device);

    /// Queries the format and present modes from a given physical device
    SwapChainSupportDetails querySwapChainSuppor(const VkPhysicalDevice& device);

    /// Choose best surface format from the list of given formats
    /// \param formats
    /// \return
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);

    /// Choose best present mode from the list of given modes
    /// \param presentModes
    /// \return
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& presentModes);

    /// Choose resolution of swap chain images
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

    void createSwapChain();

    void createSwapChainImageViews();

    [[nodiscard]] std::unique_ptr<ImageView> createImageView(const VkImage& image, VkFormat imageFormat, VkImageAspectFlagBits aspectMask = VK_IMAGE_ASPECT_COLOR_BIT) const;
};
