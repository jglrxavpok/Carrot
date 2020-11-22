//
// Created by jglrxavpok on 21/11/2020.
//

#pragma once
#include <vulkan/vulkan.h>
#include "VulkanResource.hpp"

VkResult createSinglePipeline(VkDevice device, const VkGraphicsPipelineCreateInfo* createInfo, const VkAllocationCallbacks* allocator, VkPipeline* out);

typedef VulkanResource<VkSwapchainKHR, VkSwapchainCreateInfoKHR, vkCreateSwapchainKHR, vkDestroySwapchainKHR> Swapchain;
typedef VulkanResource<VkImageView, VkImageViewCreateInfo, vkCreateImageView, vkDestroyImageView> ImageView;
typedef VulkanResource<VkShaderModule, VkShaderModuleCreateInfo, vkCreateShaderModule, vkDestroyShaderModule> ShaderModule;
typedef VulkanResource<VkPipelineLayout, VkPipelineLayoutCreateInfo, vkCreatePipelineLayout, vkDestroyPipelineLayout> PipelineLayout;
typedef VulkanResource<VkRenderPass, VkRenderPassCreateInfo, vkCreateRenderPass, vkDestroyRenderPass> RenderPass;
typedef VulkanResource<VkPipeline, VkGraphicsPipelineCreateInfo, createSinglePipeline, vkDestroyPipeline> Pipeline;
typedef VulkanResource<VkFramebuffer, VkFramebufferCreateInfo, vkCreateFramebuffer, vkDestroyFramebuffer> Framebuffer;
typedef VulkanResource<VkCommandPool, VkCommandPoolCreateInfo, vkCreateCommandPool, vkDestroyCommandPool> CommandPool;
typedef VulkanResource<VkSemaphore, VkSemaphoreCreateInfo, vkCreateSemaphore, vkDestroySemaphore> Semaphore;
typedef VulkanResource<VkFence, VkFenceCreateInfo, vkCreateFence, vkDestroyFence> Fence;
