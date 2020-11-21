//
// Created by jglrxavpok on 21/11/2020.
//

#pragma once
#include <vulkan/vulkan.h>
#include "VulkanResource.hpp"

typedef VulkanResource<VkSwapchainKHR, VkSwapchainCreateInfoKHR, vkCreateSwapchainKHR, vkDestroySwapchainKHR> Swapchain;
typedef VulkanResource<VkImageView, VkImageViewCreateInfo, vkCreateImageView, vkDestroyImageView> ImageView;