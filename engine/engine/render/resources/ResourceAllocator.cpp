//
// Created by jglrxavpok on 10/03/2021.
//

#include "ResourceAllocator.h"
#include <engine/Engine.h>
#include <engine/console/RuntimeOption.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vk_enum_string_helper.h>

static vk::DeviceSize HeapSize = 1024 * 1024 * 1024; // 1GiB, no particular reason for this exact value
static Carrot::RuntimeOption ShowAllocatorDebug("Debug/Resource Allocator", false);

namespace Carrot {
    ResourceAllocator::ResourceAllocator(VulkanDriver& device): device(device) {
        stagingHeap = std::make_unique<Buffer>(device, HeapSize,
                                        vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eStorageBuffer,
                                        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                                        std::set<uint32_t>{GetVulkanDriver().getQueueFamilies().transferFamily.value()});
        stagingHeap->setDebugNames("ResourceAllocator heap for staging buffers");

        auto& driverQueueFamilies = GetVulkanDriver().getQueueFamilies();
        deviceHeap = std::make_unique<Buffer>(device, HeapSize,
                                              vk::BufferUsageFlagBits::eStorageBuffer
                                              | vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR
                                              | vk::BufferUsageFlagBits::eUniformBuffer
                                              | vk::BufferUsageFlagBits::eVertexBuffer
                                              | vk::BufferUsageFlagBits::eIndexBuffer
                                              | vk::BufferUsageFlagBits::eTransferDst
                                              ,
                                              vk::MemoryPropertyFlagBits::eDeviceLocal,
                                              std::set{driverQueueFamilies.graphicsFamily.value(), driverQueueFamilies.computeFamily.value()});
        deviceHeap->setDebugNames("ResourceAllocator heap for device buffers");

        VmaVirtualBlock stagingBlock;
        VmaVirtualBlockCreateInfo blockInfo = {};
        blockInfo.size = HeapSize;
        VkResult r = vmaCreateVirtualBlock(&blockInfo, &stagingBlock);

        verify(r == VK_SUCCESS, Carrot::sprintf("Failed to create virtual block (%s)", string_VkResult(r)));
        verify(stagingBlock != nullptr, "Failed to create virtual block");
        stagingVirtualBlock = stagingBlock;

        VmaVirtualBlock deviceBlock;
        blockInfo = {};
        blockInfo.size = HeapSize;
        r = vmaCreateVirtualBlock(&blockInfo, &deviceBlock);

        verify(r == VK_SUCCESS, Carrot::sprintf("Failed to create virtual block (%s)", string_VkResult(r)));
        verify(deviceBlock != nullptr, "Failed to create virtual block");
        deviceVirtualBlock = deviceBlock;
    }

    ResourceAllocator::~ResourceAllocator() {
        vmaDestroyVirtualBlock((VmaVirtualBlock)stagingVirtualBlock);
        vmaDestroyVirtualBlock((VmaVirtualBlock)deviceVirtualBlock);
    }

    BufferAllocation ResourceAllocator::allocateStagingBuffer(vk::DeviceSize size, vk::DeviceSize alignment) {
        auto makeDedicated = [&]() {
            BufferAllocation result { this };
            dedicatedStagingBuffers.emplace_back(allocateDedicatedBuffer(size, vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eStorageBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, std::set<uint32_t>{GetVulkanDriver().getQueueFamilies().transferFamily.value()}));
            auto& pBuffer = dedicatedStagingBuffers.back();
            result.allocation = pBuffer.get();
            result.dedicated = true;
            result.view = pBuffer->getWholeView();
            return result;
        };
        VmaVirtualAllocationCreateInfo allocInfo = {0};
        allocInfo.size = size;
        allocInfo.alignment = std::lcm(alignment, GetVulkanDriver().getPhysicalDeviceLimits().minStorageBufferOffsetAlignment);
        {
            Carrot::Async::LockGuard g{stagingAccess};
            BufferAllocation alloc = allocateInHeap(allocInfo, stagingVirtualBlock, *stagingHeap, makeDedicated);
            alloc.staging = true;
            return std::move(alloc);
        }
    }

    BufferAllocation ResourceAllocator::allocateDeviceBuffer(vk::DeviceSize size, vk::BufferUsageFlags usageFlags) {
        auto makeDedicated = [&]() {
            BufferAllocation result { this };
            auto& driverQueueFamilies = GetVulkanDriver().getQueueFamilies();
            dedicatedDeviceBuffers.emplace_back(allocateDedicatedBuffer(size, usageFlags, vk::MemoryPropertyFlagBits::eDeviceLocal, std::set{driverQueueFamilies.graphicsFamily.value(), driverQueueFamilies.computeFamily.value()}));
            auto& pBuffer = dedicatedDeviceBuffers.back();
            result.allocation = pBuffer.get();
            result.dedicated = true;
            result.view = pBuffer->getWholeView();
            return result;
        };
        VmaVirtualAllocationCreateInfo allocInfo = {0};
        allocInfo.size = size;

        if(usageFlags & vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR) {
            allocInfo.alignment = 256;
        } else if(usageFlags & vk::BufferUsageFlagBits::eUniformBuffer) {
            allocInfo.alignment = GetVulkanDriver().getPhysicalDeviceLimits().minUniformBufferOffsetAlignment;
        } else if(usageFlags & (vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eStorageBuffer)) {
            allocInfo.alignment = GetVulkanDriver().getPhysicalDeviceLimits().minStorageBufferOffsetAlignment;
        }

        {
            Carrot::Async::LockGuard g{deviceAccess};
            return allocateInHeap(allocInfo, deviceVirtualBlock, *deviceHeap, makeDedicated);
        }
    }

    void ResourceAllocator::beginFrame(const Render::Context& renderContext) {
        if(!ShowAllocatorDebug) {
            return;
        }
        if(ImGui::Begin("Resource Allocator")) {
            if(ImGui::CollapsingHeader("Staging buffers")) {
                if(ImGui::BeginChild("##staging buffers")) {

                }
                ImGui::EndChild();
            }

            if(ImGui::CollapsingHeader("Device buffers")) {
                if(ImGui::BeginChild("##device buffers", ImVec2(0,0), true)) {
                    VmaVirtualBlock block = reinterpret_cast<VmaVirtualBlock>(deviceVirtualBlock);
                    VmaStatistics stats;
                    vmaGetVirtualBlockStatistics(block, &stats);

                    ImGui::Text("Physical heap size: %llu bytes", HeapSize);
                    ImGui::Text("Virtual heap usage: %llu bytes", stats.allocationBytes);
                    ImGui::Text("Virtual allocation count: %u", stats.allocationCount);
                }
                ImGui::EndChild();
            }
        }
        ImGui::End();
    }

    BufferView ResourceAllocator::allocateBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                                                                 vk::MemoryPropertyFlags properties,
                                                                 const std::set<uint32_t>& families) {
        // TODO: proper allocation algorithm
        auto buffer = allocateDedicatedBuffer(size, usage, properties, families);
        auto result = buffer->getWholeView();
        allocatedBuffers.emplace_back(move(buffer));
        return result;
    }

    std::unique_ptr<Buffer> ResourceAllocator::allocateDedicatedBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                                                                          vk::MemoryPropertyFlags properties,
                                                                          const std::set<uint32_t>& families) {
        return std::make_unique<Buffer>(device, size, usage, properties, families);
    }

    void ResourceAllocator::freeBufferView(BufferView& view) {
        // TODO: proper allocation algorithm
        std::erase_if(allocatedBuffers, [&](const auto& p) { return p->getVulkanBuffer() == view.getBuffer().getVulkanBuffer(); });
    }

    void ResourceAllocator::freeStagingBuffer(BufferAllocation* buffer) {
        if(buffer->dedicated) {
            if(buffer->staging) {
                Carrot::Async::LockGuard g { stagingAccess };
                std::erase_if(dedicatedStagingBuffers, [&](const std::unique_ptr<Carrot::Buffer>& pBuffer) {
                    return pBuffer.get() == (Carrot::Buffer*)buffer->allocation;
                });
            } else {
                Carrot::Async::LockGuard g { deviceAccess };
                std::erase_if(dedicatedDeviceBuffers, [&](const std::unique_ptr<Carrot::Buffer>& pBuffer) {
                    return pBuffer.get() == (Carrot::Buffer*)buffer->allocation;
                });
            }
        } else {
            if(buffer->staging) {
                Carrot::Async::LockGuard g { stagingAccess };
                vmaVirtualFree((VmaVirtualBlock) stagingVirtualBlock, (VmaVirtualAllocation) buffer->allocation);
            } else {
                Carrot::Async::LockGuard g { deviceAccess };
                vmaVirtualFree((VmaVirtualBlock) deviceVirtualBlock, (VmaVirtualAllocation) buffer->allocation);
            }
        }
    }

    BufferAllocation ResourceAllocator::allocateInHeap(const VmaVirtualAllocationCreateInfo& allocInfo,
                                                       void* virtualBlock,
                                                       Carrot::Buffer& heapStorage,
                                                       std::function<BufferAllocation()> makeDedicated) {
        if(allocInfo.size > HeapSize) {
            return makeDedicated();
        }

        VmaVirtualBlock block = reinterpret_cast<VmaVirtualBlock>(virtualBlock);
        VmaVirtualAllocation alloc;
        VkDeviceSize offset;
        VkResult result = vmaVirtualAllocate(block, &allocInfo, &alloc, &offset);
        if(result == VK_SUCCESS) {
            BufferAllocation resultBuffer {this };
            resultBuffer.allocation = alloc;
            resultBuffer.dedicated = false;
            resultBuffer.view = heapStorage.getWholeView().subView(offset, allocInfo.size);
            return resultBuffer;
        } else {
            // if we reach here, no block has space available, create a dedicated buffer
            return makeDedicated();
        }
    }
}