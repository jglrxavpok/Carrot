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
                                        vk::BufferUsageFlagBits::eTransferSrc
                                        | vk::BufferUsageFlagBits::eTransferDst
                                        | vk::BufferUsageFlagBits::eStorageBuffer
                                        | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR
                                        | vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR,
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
                                              | vk::BufferUsageFlagBits::eTransferSrc
                                              | vk::BufferUsageFlagBits::eTransferDst
                                              | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR
                                              ,
                                              vk::MemoryPropertyFlagBits::eDeviceLocal,
                                              std::set{driverQueueFamilies.graphicsFamily.value(), driverQueueFamilies.computeFamily.value(), driverQueueFamilies.transferFamily.value()});
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
        ZoneScoped;
        auto makeDedicated = [&](vk::DeviceSize dedicatedSize) {
            BufferAllocation result { this };
            dedicatedStagingBuffers.emplaceBack(allocateDedicatedBuffer(dedicatedSize, vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, std::set<uint32_t>{GetVulkanDriver().getQueueFamilies().graphicsFamily.value(), GetVulkanDriver().getQueueFamilies().computeFamily.value(), GetVulkanDriver().getQueueFamilies().transferFamily.value()}));
            auto& pBuffer = dedicatedStagingBuffers.back();
            pBuffer->name("ResourceAllocator::allocateStagingBuffer");
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
        ZoneScoped;
        BufferAllocation result;
        multiAllocateDeviceBuffer({&result, 1}, { &size, 1 }, usageFlags);
        return result;
    }

    void ResourceAllocator::multiAllocateDeviceBuffer(std::size_t count, std::size_t stride, BufferAllocation* outputs, const vk::DeviceSize* sizes, vk::BufferUsageFlags usageFlags) {
        ZoneScoped;
        verify(stride != 0, "Stride must not be 0");
        auto makeDedicated = [&](vk::DeviceSize size) {
            BufferAllocation result { this };
            auto& driverQueueFamilies = GetVulkanDriver().getQueueFamilies();
            dedicatedDeviceBuffers.emplaceBack(allocateDedicatedBuffer(size, usageFlags, vk::MemoryPropertyFlagBits::eDeviceLocal, std::set{driverQueueFamilies.graphicsFamily.value(), driverQueueFamilies.computeFamily.value(), driverQueueFamilies.transferFamily.value()}));
            auto& pBuffer = dedicatedDeviceBuffers.back();
            pBuffer->name("ResourceAllocator::allocateDeviceBuffer");
            result.allocation = pBuffer.get();
            result.dedicated = true;
            result.view = pBuffer->getWholeView();
            return result;
        };
        VmaVirtualAllocationCreateInfo allocInfo = {0};

        if(usageFlags & vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR) {
            allocInfo.alignment = 256;
        } else if(usageFlags & vk::BufferUsageFlagBits::eUniformBuffer) {
            allocInfo.alignment = GetVulkanDriver().getPhysicalDeviceLimits().minUniformBufferOffsetAlignment;
        } else if(usageFlags & (vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eStorageBuffer)) {
            allocInfo.alignment = GetVulkanDriver().getPhysicalDeviceLimits().minStorageBufferOffsetAlignment;
        } else if(usageFlags & (vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR)) {
            // TODO: which value is valid?
            allocInfo.alignment = GetVulkanDriver().getPhysicalDeviceLimits().minStorageBufferOffsetAlignment;
        }

        {
            Carrot::Async::LockGuard g{deviceAccess};
            for(std::size_t i = 0; i < count; i++) {
                std::byte* outputPtr = reinterpret_cast<std::byte*>(outputs) + stride * i;
                const std::byte* sizePtr = reinterpret_cast<const std::byte*>(sizes) + stride * i;
                BufferAllocation* output = reinterpret_cast<BufferAllocation*>(outputPtr);
                const vk::DeviceSize size = *reinterpret_cast<const vk::DeviceSize*>(sizePtr);
                allocInfo.size = size;
                *output = allocateInHeap(allocInfo, deviceVirtualBlock, *deviceHeap, makeDedicated);
            }
        }
    }

    void ResourceAllocator::multiAllocateDeviceBuffer(std::span<BufferAllocation> output, std::span<const vk::DeviceSize> sizes, vk::BufferUsageFlags usageFlags) {
        ZoneScoped;
        verify(output.size() == sizes.size(), "Must be as many outputs as there are sizes");
        auto makeDedicated = [&](vk::DeviceSize size) {
            BufferAllocation result { this };
            auto& driverQueueFamilies = GetVulkanDriver().getQueueFamilies();
            dedicatedDeviceBuffers.emplaceBack(allocateDedicatedBuffer(size, usageFlags, vk::MemoryPropertyFlagBits::eDeviceLocal, std::set{driverQueueFamilies.graphicsFamily.value(), driverQueueFamilies.computeFamily.value(), driverQueueFamilies.transferFamily.value()}));
            auto& pBuffer = dedicatedDeviceBuffers.back();
            pBuffer->name("ResourceAllocator::allocateDeviceBuffer");
            result.allocation = pBuffer.get();
            result.dedicated = true;
            result.view = pBuffer->getWholeView();
            return result;
        };
        VmaVirtualAllocationCreateInfo allocInfo = {0};

        if(usageFlags & vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR) {
            allocInfo.alignment = 256;
        } else if(usageFlags & vk::BufferUsageFlagBits::eUniformBuffer) {
            allocInfo.alignment = GetVulkanDriver().getPhysicalDeviceLimits().minUniformBufferOffsetAlignment;
        } else if(usageFlags & (vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eStorageBuffer)) {
            allocInfo.alignment = GetVulkanDriver().getPhysicalDeviceLimits().minStorageBufferOffsetAlignment;
        } else if(usageFlags & (vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR)) {
            // TODO: which value is valid?
            allocInfo.alignment = GetVulkanDriver().getPhysicalDeviceLimits().minStorageBufferOffsetAlignment;
        }

        {
            Carrot::Async::LockGuard g{deviceAccess};
            for(std::size_t i = 0; i < output.size(); i++) {
                allocInfo.size = sizes[i];
                output[i] = allocateInHeap(allocInfo, deviceVirtualBlock, *deviceHeap, makeDedicated);
            }
        }
    }

    void ResourceAllocator::beginFrame(const Render::Context& renderContext) {
        currentFrame = renderContext.frameCount;

        // clear buffers that are no longer used
        for(std::int64_t index = 0; index < dedicatedBufferGraveyard.size();) {
            auto& [frameIndex, _] = dedicatedBufferGraveyard[index];
            if(frameIndex >= currentFrame) {
                dedicatedBufferGraveyard.remove(index);
            } else {
                index++;
            }
        }

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
        allocatedBuffers.emplaceBack(move(buffer));
        return result;
    }

    UniquePtr<Buffer> ResourceAllocator::allocateDedicatedBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                                                                          vk::MemoryPropertyFlags properties,
                                                                          const std::set<uint32_t>& families) {
        auto pBuffer = makeUnique<Buffer>(Allocator::getDefault(), device, size, usage, properties, families);
        pBuffer->name("ResourceAllocator::allocateDedicatedBuffer");
        return pBuffer;
    }

    void ResourceAllocator::freeBufferView(BufferView& view) {
        // TODO: proper allocation algorithm
        allocatedBuffers.removeIf([&](const auto& p) { return p->getVulkanBuffer() == view.getBuffer().getVulkanBuffer(); });
    }

    void ResourceAllocator::freeStagingBuffer(BufferAllocation* buffer) {
        if(buffer->dedicated) {
            // it is possible that the buffer is deleted from the "tick"/"logic" part of the engine, but the renderer still uses it
            //  so don't delete the buffer immediately
            auto moveToGraveyard = [&](Vector<UniquePtr<Buffer>>& buffers) {
                for(std::int64_t index = 0; index < buffers.size();) {
                    auto& pBuffer = buffers[index];
                    if(pBuffer.get() == (Carrot::Buffer*)buffer->allocation) {
                        dedicatedBufferGraveyard.emplaceBack(Pair{ currentFrame+3, std::move(pBuffer) });
                        buffers.remove(index);
                    } else {
                        index++;
                    }
                }
            };
            if(buffer->staging) {
                Carrot::Async::LockGuard g { stagingAccess };
                moveToGraveyard(dedicatedStagingBuffers);
            } else {
                Carrot::Async::LockGuard g { deviceAccess };
                moveToGraveyard(dedicatedDeviceBuffers);
            }
        } else {
            // TODO: add a graveyard for virtual blocks too?
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
                                                       std::function<BufferAllocation(vk::DeviceSize)> makeDedicated) {
        if(allocInfo.size > HeapSize) {
            // TODO: support for multiple heaps?
            return makeDedicated(allocInfo.size);
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
           // resultBuffer.name("Unnamed suballoc");
            return resultBuffer;
        } else {
            // if we reach here, no block has space available, create a dedicated buffer
            return makeDedicated(allocInfo.size);
        }
    }
}