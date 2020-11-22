//
// Created by jglrxavpok on 22/11/2020.
//
#include <vulkan/vulkan.h>
#include "types.h"


VkResult createSinglePipeline(VkDevice device, const VkGraphicsPipelineCreateInfo* createInfo, const VkAllocationCallbacks* allocator, VkPipeline* out) {
    return vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, createInfo, allocator, out);
}