//
// Created by jglrxavpok on 21/11/2020.
//
#pragma once
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include "memory/NakedPtr.hpp"

using namespace std;

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

    void init();
    void initWindow();
    void initVulkan();
    void cleanup();

    void createInstance();

    bool checkValidationLayerSupport();

    std::vector<const char *> getRequiredExtensions();

    void setupDebugMessenger();

    void setupMessenger(VkDebugUtilsMessengerCreateInfoEXT &ext);
};
