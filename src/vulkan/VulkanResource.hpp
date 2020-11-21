//
// Created by jglrxavpok on 21/11/2020.
//

#pragma once
#include <vulkan/vulkan.h>
#include <algorithm>
#include <memory>

/// Device-based resource that will be cleared inside this class' destructor
/// <br/>
/// Think of this as a smart pointer of Vulkan device resources
/// \tparam VulkanType Vulkan type to store
/// \tparam Destructor Destructor method
template<typename VulkanType,
        typename CreateStruct,
        VkResult Constructor(VkDevice, const CreateStruct*, const VkAllocationCallbacks*, VulkanType*),
        void Destructor(VkDevice, VulkanType, const VkAllocationCallbacks*)
        >
class VulkanResource {
private:
    VulkanType resource;
    const VkDevice& device;
    const VkAllocationCallbacks* allocator;

public:
    VulkanResource(const VkDevice& device, const VkAllocationCallbacks* allocator, VulkanType resource): device(device), allocator(allocator), resource(resource)
    {}

    operator VulkanType() const {
        return resource;
    }

    VulkanType get() const {
        return resource;
    }

    virtual ~VulkanResource() {
        Destructor(device, resource, allocator);
    };

    static std::unique_ptr<VulkanResource> createUnique(const VkDevice& device, const CreateStruct& createStruct, const VkAllocationCallbacks* allocator, std::string errorMessage = "Failed to create resource!") {
        VulkanType out;
        VkResult result = Constructor(device, &createStruct, allocator, &out);
        if(result != VK_SUCCESS) {
            throw std::runtime_error(errorMessage);
        }
        return make_unique<VulkanResource>(device, allocator, out);
    }

    static std::shared_ptr<VulkanResource> createShared(const VkDevice& device, const CreateStruct& createStruct, const VkAllocationCallbacks* allocator, std::string errorMessage = "Failed to create resource!") {
        VulkanType out;
        VkResult result = Constructor(device, &createStruct, allocator, &out);
        if(result != VK_SUCCESS) {
            throw std::runtime_error(errorMessage);
        }
        return make_shared<VulkanResource>(device, allocator, out);
    }

};